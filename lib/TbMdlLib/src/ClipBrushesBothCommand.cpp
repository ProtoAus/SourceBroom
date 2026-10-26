/*
 Copyright (C) 2026 ProtoAus
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mdl/ClipBrushesBothCommand.h"

#include "base/Logger.h"
#include "mdl/AddRemoveNodesUtils.h"
#include "mdl/BrushFace.h"
#include "mdl/BrushNode.h"
#include "mdl/EditorContext.h"
#include "mdl/Map.h"
#include "mdl/SelectionCommand.h"
#include "mdl/SwapNodeContentsUtils.h"
#include "mdl/VmfScene.h"

#include <stdexcept>

namespace tb::mdl
{
namespace
{
bool sameBrush(const Brush& a, const Brush& b)
{
  if (a.faceCount() != b.faceCount())
  {
    return false;
  }
  for (size_t i = 0; i < a.faceCount(); ++i)
  {
    const auto& x = a.faces()[i];
    const auto& y = b.faces()[i];
    const auto xu = x.takeUvCoordSystemSnapshot();
    const auto yu = y.takeUvCoordSystemSnapshot();
    if (
      x.lineNumber() != y.lineNumber() || x.points() != y.points()
      || x.materialName() != y.materialName() || x.uvAttributes() != y.uvAttributes()
      || x.surfaceAttributes() != y.surfaceAttributes() || !xu || !yu
      || xu->uAxis != yu->uAxis || xu->vAxis != yu->vAxis || x.sourceDisplacement()
      || y.sourceDisplacement())
    {
      return false;
    }
  }
  return true;
}
} // namespace

ClipBrushesBothCommand::ClipBrushesBothCommand(
  std::vector<BrushNode*> subjects,
  std::vector<std::pair<Node*, NodeContents>> keepers,
  std::vector<std::unique_ptr<BrushNode>> siblings)
  : UndoableCommand{"Clip Brushes Both", true}
  , m_subjects{std::move(subjects)}
  , m_keepers{std::move(keepers)}
  , m_siblingOwners{std::move(siblings)}
{
  for (const auto* subject : m_subjects)
  {
    m_originals.push_back(subject->brush());
    m_parents.push_back(subject->parent());
  }
  for (const auto& sibling : m_siblingOwners)
  {
    m_siblings.push_back(sibling.get());
  }
  for (size_t i = 0; i < m_keepers.size(); ++i)
  {
    m_additions[m_keepers[i].first->parent()].push_back(m_siblings[i]);
  }
}

ClipBrushesBothCommand::~ClipBrushesBothCommand() = default;

void ClipBrushesBothCommand::ownDetachedSiblings()
{
  // Called while every raw sibling is alive, also on partial tree-edit unwind.
  // Attached nodes belong exclusively to the tree, never to our unique_ptrs.
  for (size_t i = 0; i < m_siblings.size(); ++i)
  {
    if (!m_siblings[i]->parent())
    {
      if (!m_siblingOwners[i])
      {
        m_siblingOwners[i].reset(m_siblings[i]);
      }
    }
    else
    {
      m_siblingOwners[i].release();
    }
  }
}

bool ClipBrushesBothCommand::doPerformDo(Map& map)
{
  try
  {
    auto* scene = map.vmfScene();
    if (!scene)
    {
      return false;
    }
    for (size_t i = 0; i < m_subjects.size(); ++i)
    {
      if (
        m_subjects[i]->parent() != m_parents[i]
        || !map.editorContext().visible(*m_subjects[i])
        || !sameBrush(m_subjects[i]->brush(), m_originals[i]))
      {
        throw std::invalid_argument{"native Both original changed"};
      }
    }
    if (!m_registered)
    {
      // Allocate all command snapshots/selection helpers before scene publication.
      m_deselect = SelectionCommand::deselectAll();
      auto selected = std::vector<Node*>{m_subjects.begin(), m_subjects.end()};
      selected.insert(selected.end(), m_siblings.begin(), m_siblings.end());
      m_select = SelectionCommand::select(std::move(selected));
      m_backStates.reserve(m_siblings.size());
      for (const auto* sibling : m_siblings)
      {
        m_backStates.push_back(sibling->brush());
      }
      scene->registerBothClips(m_subjects, m_keepers, m_siblings, map.worldBounds());
      // Identity assignment cannot change face order or allocate Brush storage.
      for (size_t i = 0; i < m_siblings.size(); ++i)
      {
        for (size_t j = 0; j < m_backStates[i].faceCount(); ++j)
        {
          m_backStates[i].faces()[j].setFilePosition(
            m_siblings[i]->brush().faces()[j].lineNumber(), 1);
        }
      }
      m_registered = true;
    }
    else
    {
      for (size_t i = 0; i < m_siblings.size(); ++i)
      {
        if (!sameBrush(m_siblings[i]->brush(), m_backStates[i]))
        {
          throw std::invalid_argument{"native Both parked sibling changed"};
        }
      }
      scene->validateBothClipReplay(m_subjects, m_keepers, m_siblings, map.worldBounds());
    }
  }
  catch (const std::exception& e)
  {
    map.logger().error() << "Native VMF Both clip refused: " << e.what();
    return false;
  }

  m_deselect->performDo(map);
  swapNodeContentsAndNotify(m_keepers, map);
  for (auto& sibling : m_siblingOwners)
  {
    sibling.release();
  }
  try
  {
    addNodesAndNotify(m_additions, map);
  }
  catch (...)
  {
    ownDetachedSiblings();
    throw;
  }
  ownDetachedSiblings();
  m_select->performDo(map);
  return true;
}

bool ClipBrushesBothCommand::doPerformUndo(Map& map)
{
  m_select->performUndo(map);
  try
  {
    removeNodesAndNotify(m_additions, map);
  }
  catch (...)
  {
    ownDetachedSiblings();
    throw;
  }
  ownDetachedSiblings();
  swapNodeContentsAndNotify(m_keepers, map);
  m_deselect->performUndo(map);
  return true;
}
} // namespace tb::mdl
