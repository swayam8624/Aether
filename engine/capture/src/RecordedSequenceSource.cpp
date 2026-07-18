#include <aether/capture/RecordedSequenceSource.hpp>

#include <simdjson.h>

#include <array>
#include <cmath>
#include <fstream>
#include <limits>
#include <mutex>
#include <string_view>
#include <utility>
#include <vector>

namespace aether::capture {
namespace {

constexpr std::size_t maximumFrames = 1'000'000;
constexpr std::uint32_t maximumDimension = 16'384;
constexpr std::size_t maximumPlaneBytes = 512ULL * 1024ULL * 1024ULL;

struct FrameRecord final {
    std::uint64_t frameId{};
    std::uint64_t timestampNs{};
    std::filesystem::path colorPath;
    std::filesystem::path depthPath;
    std::optional<std::filesystem::path> confidencePath;
    RigidPose pose;
};

Result<std::string> stringField(simdjson::dom::element object, const char* field) {
    std::string_view value;
    if (object[field].get(value))
        return fail(ErrorCode::corruptData, "Capture manifest string field is invalid", field);
    return std::string(value);
}

Result<std::uint64_t> uintField(simdjson::dom::element object, const char* field) {
    std::uint64_t value{};
    if (object[field].get(value))
        return fail(ErrorCode::corruptData, "Capture manifest integer field is invalid", field);
    return value;
}

Result<double> numberField(simdjson::dom::element object, const char* field) {
    double value{};
    if (object[field].get(value) || !std::isfinite(value))
        return fail(ErrorCode::corruptData, "Capture manifest number field is invalid", field);
    return value;
}

Result<std::filesystem::path> relativePath(simdjson::dom::element object, const char* field) {
    auto text = stringField(object, field);
    if (!text)
        return std::unexpected(text.error());
    std::filesystem::path path(*text);
    if (path.empty() || path.is_absolute())
        return fail(ErrorCode::corruptData, "Capture path must be relative", field);
    for (const auto& component : path)
        if (component == "..")
            return fail(ErrorCode::corruptData, "Capture path traversal is forbidden", field);
    return path.lexically_normal();
}

template <std::size_t Size>
Result<std::array<double, Size>> numberArray(simdjson::dom::element object, const char* field) {
    simdjson::dom::array values;
    if (object[field].get_array().get(values) || values.size() != Size)
        return fail(ErrorCode::corruptData, "Capture manifest array has invalid length", field);
    std::array<double, Size> result{};
    std::size_t index = 0;
    for (auto value : values) {
        if (value.get(result[index]) || !std::isfinite(result[index]))
            return fail(ErrorCode::corruptData, "Capture manifest array contains invalid value",
                        field);
        ++index;
    }
    return result;
}

Result<std::vector<std::byte>> readExact(const std::filesystem::path& path,
                                         std::size_t expectedBytes) {
    if (expectedBytes == 0 || expectedBytes > maximumPlaneBytes)
        return fail(ErrorCode::resourceExhausted, "Recorded plane exceeds byte budget",
                    path.string());
    std::error_code error;
    const auto fileBytes = std::filesystem::file_size(path, error);
    if (error || fileBytes != expectedBytes)
        return fail(ErrorCode::corruptData, "Recorded plane byte count does not match manifest",
                    path.string());
    std::vector<std::byte> bytes(expectedBytes);
    std::ifstream stream(path, std::ios::binary);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size())))
        return fail(ErrorCode::io, "Unable to read recorded plane", path.string());
    return bytes;
}

} // namespace

class RecordedSequenceSource::Impl final {
public:
    std::filesystem::path root;
    RecordedPlaybackConfig config;
    CameraCalibration calibration;
    FrameSourceInfo info;
    std::vector<FrameRecord> frames;
    std::size_t cursor{};
    bool running{};
    PacketCallback packetCallback;
    ErrorCallback errorCallback;
    mutable std::mutex mutex;

    Result<CapturePacket> loadPacket(std::size_t index) const {
        const auto& record = frames[index];
        const auto pixelCount =
            static_cast<std::size_t>(calibration.width) * calibration.height;
        if (calibration.width != 0 &&
            pixelCount / calibration.width != calibration.height)
            return fail(ErrorCode::resourceExhausted, "Recorded image dimensions overflow");

        auto color = readExact(root / record.colorPath, pixelCount * 3);
        if (!color)
            return std::unexpected(color.error());
        auto depth = readExact(root / record.depthPath, pixelCount * sizeof(float));
        if (!depth)
            return std::unexpected(depth.error());

        CapturePacket packet;
        packet.frameId = record.frameId;
        packet.sourceId = info.sourceId;
        packet.sourceKind = CaptureSourceKind::recordedRgbd;
        packet.presentationTimestampNs = record.timestampNs;
        packet.hostTimestampNs = record.timestampNs;
        packet.calibration = calibration;
        packet.cameraToWorld = record.pose;
        packet.colorPlanes.push_back(ImagePlane{
            makeOwnedBuffer(std::move(*color)),
            PixelFormat::rgb8,
            calibration.width,
            calibration.height,
            calibration.width * 3,
        });
        packet.depthMetres = ImagePlane{
            makeOwnedBuffer(std::move(*depth)),
            PixelFormat::depthFloat32Metres,
            calibration.width,
            calibration.height,
            calibration.width * static_cast<std::uint32_t>(sizeof(float)),
        };
        if (record.confidencePath) {
            auto confidence = readExact(root / *record.confidencePath, pixelCount);
            if (!confidence)
                return std::unexpected(confidence.error());
            packet.depthConfidence = ImagePlane{
                makeOwnedBuffer(std::move(*confidence)),
                PixelFormat::confidenceUInt8,
                calibration.width,
                calibration.height,
                calibration.width,
            };
        }
        return packet;
    }
};

Result<std::unique_ptr<RecordedSequenceSource>>
RecordedSequenceSource::open(const std::filesystem::path& directory,
                             RecordedPlaybackConfig config) {
    const auto manifestPath = directory / "manifest.json";
    simdjson::dom::parser parser;
    auto documentResult = parser.load(manifestPath.string());
    if (documentResult.error())
        return fail(ErrorCode::corruptData, "Unable to parse capture manifest",
                    manifestPath.string());
    simdjson::dom::element document = documentResult.value();

    auto schemaVersion = uintField(document, "schemaVersion");
    if (!schemaVersion || *schemaVersion != 1)
        return fail(ErrorCode::unsupported, "Unsupported capture schema version",
                    schemaVersion ? std::to_string(*schemaVersion) : "missing");

    auto impl = std::make_unique<Impl>();
    impl->root = directory;
    impl->config = config;
    auto sourceId = stringField(document, "sourceId");
    if (!sourceId)
        return std::unexpected(sourceId.error());
    impl->info = FrameSourceInfo{*sourceId, CaptureSourceKind::recordedRgbd,
                                 "Recorded RGB-D sequence", true, true, false};

    simdjson::dom::element calibrationObject;
    if (document["calibration"].get(calibrationObject))
        return fail(ErrorCode::corruptData, "Capture calibration is missing");
    auto width = uintField(calibrationObject, "width");
    auto height = uintField(calibrationObject, "height");
    auto fx = numberField(calibrationObject, "fx");
    auto fy = numberField(calibrationObject, "fy");
    auto cx = numberField(calibrationObject, "cx");
    auto cy = numberField(calibrationObject, "cy");
    if (!width || !height || !fx || !fy || !cx || !cy)
        return fail(ErrorCode::corruptData, "Capture calibration is incomplete");
    if (*width == 0 || *height == 0 || *width > maximumDimension || *height > maximumDimension ||
        *fx <= 0.0 || *fy <= 0.0)
        return fail(ErrorCode::corruptData, "Capture calibration is out of bounds");
    impl->calibration.id = *sourceId;
    impl->calibration.width = static_cast<std::uint32_t>(*width);
    impl->calibration.height = static_cast<std::uint32_t>(*height);
    impl->calibration.fx = *fx;
    impl->calibration.fy = *fy;
    impl->calibration.cx = *cx;
    impl->calibration.cy = *cy;

    simdjson::dom::array frames;
    if (document["frames"].get_array().get(frames) || frames.size() == 0 ||
        frames.size() > maximumFrames)
        return fail(ErrorCode::corruptData, "Capture frame list is empty or exceeds its budget");
    std::uint64_t previousFrameId = 0;
    std::uint64_t previousTimestamp = 0;
    for (auto frameObject : frames) {
        FrameRecord record;
        auto frameId = uintField(frameObject, "frameId");
        auto timestamp = uintField(frameObject, "timestampNs");
        auto colorPath = relativePath(frameObject, "color");
        auto depthPath = relativePath(frameObject, "depth");
        auto orientation = numberArray<4>(frameObject, "orientation");
        auto translation = numberArray<3>(frameObject, "translation");
        if (!frameId || !timestamp || !colorPath || !depthPath || !orientation || !translation)
            return fail(ErrorCode::corruptData, "Capture frame entry is incomplete");
        if (*frameId <= previousFrameId || *timestamp < previousTimestamp)
            return fail(ErrorCode::corruptData,
                        "Capture frame IDs and timestamps must be ordered");
        record.frameId = *frameId;
        record.timestampNs = *timestamp;
        record.colorPath = *colorPath;
        record.depthPath = *depthPath;
        record.pose.orientation = *orientation;
        record.pose.translation = *translation;
        auto confidenceField = frameObject["confidence"];
        if (confidenceField.error() != simdjson::NO_SUCH_FIELD) {
            std::string_view confidenceText;
            if (confidenceField.get(confidenceText))
                return fail(ErrorCode::corruptData,
                            "Confidence path must be a string when present");
            std::filesystem::path confidence(confidenceText);
            if (confidence.empty() || confidence.is_absolute())
                return fail(ErrorCode::corruptData, "Confidence path must be relative");
            for (const auto& component : confidence)
                if (component == "..")
                    return fail(ErrorCode::corruptData,
                                "Confidence path traversal is forbidden");
            record.confidencePath = confidence.lexically_normal();
        }
        impl->frames.push_back(std::move(record));
        previousFrameId = *frameId;
        previousTimestamp = *timestamp;
    }
    return std::unique_ptr<RecordedSequenceSource>(
        new RecordedSequenceSource(std::move(impl)));
}

RecordedSequenceSource::RecordedSequenceSource(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}
RecordedSequenceSource::~RecordedSequenceSource() = default;

Result<FrameSourceInfo> RecordedSequenceSource::start() {
    std::lock_guard lock(impl_->mutex);
    impl_->running = true;
    return impl_->info;
}

Result<void> RecordedSequenceSource::stop() {
    std::lock_guard lock(impl_->mutex);
    impl_->running = false;
    return {};
}

void RecordedSequenceSource::setPacketCallback(PacketCallback callback) {
    std::lock_guard lock(impl_->mutex);
    impl_->packetCallback = std::move(callback);
}

void RecordedSequenceSource::setErrorCallback(ErrorCallback callback) {
    std::lock_guard lock(impl_->mutex);
    impl_->errorCallback = std::move(callback);
}

Result<bool> RecordedSequenceSource::step() {
    std::size_t index{};
    PacketCallback callback;
    ErrorCallback errorCallback;
    {
        std::lock_guard lock(impl_->mutex);
        if (!impl_->running)
            return fail(ErrorCode::invalidArgument, "Recorded source is not running");
        if (impl_->cursor >= impl_->frames.size())
            return false;
        index = impl_->cursor++;
        callback = impl_->packetCallback;
        errorCallback = impl_->errorCallback;
    }
    if (impl_->config.injectedFailureFrame &&
        index == *impl_->config.injectedFailureFrame) {
        auto error = Error{ErrorCode::io, "Injected recorded-source failure",
                           std::to_string(index)};
        if (errorCallback)
            errorCallback(error);
        return std::unexpected(std::move(error));
    }
    auto packet = impl_->loadPacket(index);
    if (!packet) {
        if (errorCallback)
            errorCallback(packet.error());
        return std::unexpected(packet.error());
    }
    if (callback)
        callback(std::move(*packet));
    return true;
}

void RecordedSequenceSource::rewind() noexcept {
    std::lock_guard lock(impl_->mutex);
    impl_->cursor = 0;
}

std::size_t RecordedSequenceSource::frameCount() const noexcept {
    return impl_->frames.size();
}

std::size_t RecordedSequenceSource::position() const noexcept {
    std::lock_guard lock(impl_->mutex);
    return impl_->cursor;
}

} // namespace aether::capture
