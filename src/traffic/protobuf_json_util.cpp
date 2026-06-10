#include "protobuf_json_util.h"

#include <QJsonDocument>

#include <google/protobuf/json/json.h>

#include "com_pet.pb.h"

namespace app {

QJsonObject protobufToJsonObject(const google::protobuf::Message &message)
{
    google::protobuf::json::PrintOptions options;
    options.preserve_proto_field_names = true;
    options.always_print_enums_as_ints = true;

    std::string json;
    const auto status = google::protobuf::json::MessageToJsonString(message, &json, options);
    if (!status.ok()) {
        return {};
    }

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    return doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonArray protobufPetDataListToJsonArray(
    const google::protobuf::RepeatedPtrField<Next::PetData> &pets)
{
    QJsonArray array;
    for (const Next::PetData &pet : pets) {
        array.append(protobufToJsonObject(pet));
    }
    return array;
}

} // namespace app
