#pragma once

#include <QJsonArray>
#include <QJsonObject>

#include <google/protobuf/message.h>
#include <google/protobuf/repeated_ptr_field.h>

namespace Next {
class PetData;
} // namespace Next

namespace app {

QJsonObject protobufToJsonObject(const google::protobuf::Message &message);
QJsonArray protobufPetDataListToJsonArray(
    const google::protobuf::RepeatedPtrField<Next::PetData> &pets);

} // namespace app
