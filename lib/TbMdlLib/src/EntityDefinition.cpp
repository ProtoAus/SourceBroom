/*
 Copyright (C) 2010 Kristian Duske

 This file is part of TrenchBroom.

 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mdl/EntityDefinition.h"

#include "kd/reflection_impl.h"
#include "kd/string_compare.h"
#include "kd/string_compare_natural.h"

#include "vm/bbox_io.h" // IWYU pragma: keep

namespace tb::mdl
{

kdl_reflect_impl(PointEntityDefinition);

size_t EntityDefinition::usageCount() const
{
  return *m_usageCount;
}

void EntityDefinition::incUsageCount() const
{
  ++*m_usageCount;
}

void EntityDefinition::decUsageCount() const
{
  --*m_usageCount;
}

kdl_reflect_impl(EntityDefinition);

PropertyDefinition* getPropertyDefinition(
  EntityDefinition& entityDefinition, const std::string& key)
{
  auto* firstMatch = static_cast<PropertyDefinition*>(nullptr);
  for (auto& property : entityDefinition.propertyDefinitions)
  {
    if (property.key == key)
    {
      if (
        !std::holds_alternative<PropertyValueTypes::Input>(property.valueType)
        && !std::holds_alternative<PropertyValueTypes::Output>(property.valueType))
      {
        return &property;
      }
      if (!firstMatch)
      {
        firstMatch = &property;
      }
    }
  }
  return firstMatch;
}

const PropertyDefinition* getPropertyDefinition(
  const EntityDefinition& entityDefinition, const std::string& key)
{
  return getPropertyDefinition(const_cast<EntityDefinition&>(entityDefinition), key);
}

const PropertyDefinition* getPropertyDefinition(
  const EntityDefinition* entityDefinition, const std::string& key)
{
  return entityDefinition ? getPropertyDefinition(*entityDefinition, key) : nullptr;
}

PropertyDefinition* getKeyValuePropertyDefinition(
  EntityDefinition& entityDefinition, const std::string& key)
{
  auto* property = getPropertyDefinition(entityDefinition, key);
  return property
             && !std::holds_alternative<PropertyValueTypes::Input>(property->valueType)
             && !std::holds_alternative<PropertyValueTypes::Output>(property->valueType)
           ? property
           : nullptr;
}

const PropertyDefinition* getKeyValuePropertyDefinition(
  const EntityDefinition& entityDefinition, const std::string& key)
{
  return getKeyValuePropertyDefinition(
    const_cast<EntityDefinition&>(entityDefinition), key);
}

std::string_view getShortName(const EntityDefinition& entityDefinition)
{
  const auto index = entityDefinition.name.find_first_of('_');
  return index == std::string::npos
           ? std::string_view{entityDefinition.name}
           : std::string_view{entityDefinition.name}.substr(index + 1);
}

bool compareEntityDefinitions(const EntityDefinition& lhs, const EntityDefinition& rhs)
{
  if (
    const auto compareResult =
      kdl::ci::str_compare_natural(getShortName(lhs), getShortName(rhs));
    compareResult != 0)
  {
    return compareResult < 0;
  }

  return kdl::cs::str_compare(lhs.name, rhs.name) < 0;
}

std::string_view getGroupName(const EntityDefinition& entityDefinition)
{
  const auto index = entityDefinition.name.find_first_of('_');
  return index == std::string::npos
           ? std::string_view{entityDefinition.name}
           : std::string_view{entityDefinition.name}.substr(0, index);
}

EntityDefinitionType getType(const EntityDefinition& entityDefinition)
{
  return entityDefinition.pointEntityDefinition ? EntityDefinitionType::Point
                                                : EntityDefinitionType::Brush;
}

const PointEntityDefinition* getPointEntityDefinition(
  const EntityDefinition* entityDefinition)
{
  return entityDefinition && entityDefinition->pointEntityDefinition
           ? &*entityDefinition->pointEntityDefinition
           : nullptr;
}

} // namespace tb::mdl
