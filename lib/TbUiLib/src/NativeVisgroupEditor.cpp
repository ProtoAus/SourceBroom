/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/NativeVisgroupEditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/WorldNode.h"
#include "ui/MapDocument.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>

namespace tb::ui
{
namespace
{
QString kindText(const mdl::NativeVisgroupMemberKind kind)
{
  switch (kind)
  {
  case mdl::NativeVisgroupMemberKind::WorldSolid:
    return QObject::tr("World solid");
  case mdl::NativeVisgroupMemberKind::PointEntity:
    return QObject::tr("Point entity");
  case mdl::NativeVisgroupMemberKind::BrushEntity:
    return QObject::tr("Whole brush entity");
  case mdl::NativeVisgroupMemberKind::InternalSolid:
    return QObject::tr("Internal solid");
  case mdl::NativeVisgroupMemberKind::Group:
    return QObject::tr("Native group");
  case mdl::NativeVisgroupMemberKind::Unsupported:
    return QObject::tr("Unsupported");
  }
  return {};
}
QString flagText(const std::optional<bool> flag)
{
  return !flag   ? QObject::tr("not set")
         : *flag ? QObject::tr("shown")
                 : QObject::tr("hidden");
}
QString rgbText(const mdl::NativeVisgroupRecord& record)
{
  if (!record.colorRefusalReason.empty())
  {
    return QObject::tr("preserved / unsupported");
  }
  if (!record.color)
  {
    return QObject::tr("not stored");
  }
  const auto& color = *record.color;
  return QString::number(color[0]) + " " + QString::number(color[1]) + " "
         + QString::number(color[2]);
}
std::string decimalKey(const std::string& id)
{
  const auto first = id.find_first_not_of('0');
  return first == std::string::npos ? "0" : id.substr(first);
}
QString causes(const mdl::NativeVisibility& mask)
{
  auto result = QStringList{};
  if (mask.wrapper)
  {
    result << QObject::tr("wrapper");
  }
  if (mask.manual)
  {
    result << QObject::tr("manual");
  }
  if (mask.automatic)
  {
    result << QObject::tr("automatic");
  }
  return result.empty() ? QObject::tr("none") : result.join(", ");
}
} // namespace

NativeVisgroupEditor::NativeVisgroupEditor(MapDocument& document, QWidget* parent)
  : QWidget{parent}
  , m_document{document}
{
  auto* outer = new QVBoxLayout{this};
  outer->setContentsMargins(0, 0, 0, 0);
  auto* scroll = new QScrollArea{};
  scroll->setWidgetResizable(true);
  scroll->setMaximumHeight(400);
  auto* content = new QWidget{};
  scroll->setWidget(content);
  outer->addWidget(scroll);
  auto* layout = new QVBoxLayout{content};
  auto* title = new QLabel{tr("LIVE native manual visgroups — includes UNSAVED changes")};
  title->setWordWrap(true);
  layout->addWidget(title);
  m_tree = new QTreeWidget{};
  m_tree->setObjectName("visgroupTree");
  m_tree->setHeaderLabels({tr("Visgroup"), tr("Native ID / protection"), tr("RGB")});
  m_tree->setMinimumHeight(100);
  layout->addWidget(m_tree);
  auto* form = new QFormLayout{};
  m_name = new QLineEdit{};
  m_name->setObjectName("visgroupName");
  form->addRow(tr("Name (explicit edits only)"), m_name);
  auto* rgb = new QWidget{};
  auto* channels = new QHBoxLayout{rgb};
  channels->setContentsMargins(0, 0, 0, 0);
  const auto names = std::array{"visgroupRed", "visgroupGreen", "visgroupBlue"};
  const auto labels = std::array{tr("R"), tr("G"), tr("B")};
  for (size_t i = 0; i < m_color.size(); ++i)
  {
    m_color[i] = new QSpinBox{};
    m_color[i]->setObjectName(names[i]);
    m_color[i]->setRange(0, 255);
    channels->addWidget(new QLabel{labels[i]});
    channels->addWidget(m_color[i]);
  }
  form->addRow(tr("RGB (explicit Set only)"), rgb);
  m_colorState = new QLabel{};
  m_colorState->setObjectName("visgroupColorState");
  m_colorState->setWordWrap(true);
  form->addRow(m_colorState);
  m_parent = new QComboBox{};
  m_parent->setObjectName("visgroupParent");
  m_parent->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
  m_parent->setMinimumContentsLength(20);
  form->addRow(tr("Parent / new child destination"), m_parent);
  layout->addLayout(form);
  const auto button = [&](const char* name, const QString& text) {
    auto* result = new QPushButton{text};
    result->setObjectName(name);
    layout->addWidget(result);
    return result;
  };
  m_create = button("visgroupCreate", tr("Create visgroup (under chosen parent)"));
  m_apply = button("visgroupApply", tr("Apply name / reparent"));
  m_setColor = button("visgroupSetColor", tr("Set RGB on this visgroup only"));
  m_delete = button("visgroupDelete", tr("Delete subtree and its supported references"));
  m_show = button("visgroupShow", tr("Show members"));
  m_hide = button("visgroupHide", tr("Hide members"));
  auto* explanation = new QLabel{tr(
    "Sets the aggregate manual visibility flag on this group’s supported member closure. "
    "It does not change hidden wrappers, automatic-hidden state, or ordinary session "
    "visibility. Not a saved Hammer checkbox. Overlapping operations: last explicit "
    "action wins per object; assigning/reparenting does not replay an old action.")};
  explanation->setWordWrap(true);
  layout->addWidget(explanation);
  m_summary = new QLabel{};
  m_summary->setObjectName("visgroupSummary");
  m_summary->setWordWrap(true);
  layout->addWidget(m_summary);
  m_members = new QTreeWidget{};
  m_members->setObjectName("visgroupMembers");
  m_members->setHeaderLabels(
    {tr("Member / owner"), tr("Direct references"), tr("Visibility / protection")});
  m_members->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_members->setMinimumHeight(100);
  layout->addWidget(m_members);
  m_assignSelection = button(
    "visgroupAssignSelection",
    tr("Assign current selection (selected native roots + descendants)"));
  m_unassignSelection = button(
    "visgroupUnassignSelection",
    tr("Unassign current selection (selected native roots + descendants)"));
  m_assignMembers = button(
    "visgroupAssignMembers", tr("Assign highlighted member handles (including hidden)"));
  m_unassignMembers =
    button("visgroupUnassignMembers", tr("Unassign highlighted member handles"));
  m_status = new QLabel{};
  m_status->setObjectName("visgroupStatus");
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
  connect(m_tree, &QTreeWidget::currentItemChanged, this, [this] { showRows(); });
  m_connections += m_document.documentWasLoadedNotifier.connect([this] { refresh(); });
  m_connections += m_document.documentDidChangeNotifier.connect([this] { refresh(); });
  m_connections += m_document.nodeVisibilityDidChangeNotifier.connect(
    [this](const auto&) { refresh(); });
  refresh();
}

int NativeVisgroupEditor::recordIndex() const
{
  const auto* item = m_tree->currentItem();
  return item ? item->data(0, Qt::UserRole).toInt() : -1;
}

void NativeVisgroupEditor::refresh()
{
  const auto oldIndex = recordIndex();
  const auto oldId = oldIndex >= 0
                       ? std::optional{m_view.metadata.records[size_t(oldIndex)].id}
                       : std::nullopt;
  auto selectedMembers = std::set<mdl::NativeVisgroupMemberId>{};
  auto currentMember = std::optional<mdl::NativeVisgroupMemberId>{};
  if (const auto* item = m_members->currentItem())
  {
    currentMember =
      m_view.metadata.members[size_t(item->data(0, Qt::UserRole).toInt())].id;
  }
  for (const auto* item : m_members->selectedItems())
  {
    selectedMembers.insert(
      m_view.metadata.members[size_t(item->data(0, Qt::UserRole).toInt())].id);
  }
  m_view = mdl::nativeVisgroupView(m_document.map());
  setVisible(m_document.map().worldNode().mapFormat() == mdl::MapFormat::SourceVMF);
  const QSignalBlocker treeBlock{m_tree};
  const QSignalBlocker memberBlock{m_members};
  m_tree->clear();
  auto items = std::vector<QTreeWidgetItem*>{};
  auto treeIndices = std::map<mdl::NativeVisgroupId, size_t>{};
  for (size_t i = 0; i < m_view.metadata.records.size(); ++i)
  {
    const auto& record = m_view.metadata.records[i];
    auto* item = new QTreeWidgetItem{QStringList{
      QString::fromUtf8(record.name),
      QString::fromStdString(
        record.nativeId
        + (record.refusalReason.empty() ? "" : " — " + record.refusalReason)),
      rgbText(record)}};
    item->setData(0, Qt::UserRole, int(i));
    items.push_back(item);
    treeIndices.emplace(record.id, i);
  }
  for (size_t i = 0; i < items.size(); ++i)
  {
    const auto& parent = m_view.metadata.records[i].parent;
    const auto found = parent ? treeIndices.find(*parent) : treeIndices.end();
    if (found == treeIndices.end())
    {
      m_tree->addTopLevelItem(items[i]);
    }
    else
    {
      items[found->second]->addChild(items[i]);
    }
  }
  m_tree->expandAll();
  auto* current = items.empty() ? nullptr : items.front();
  if (oldId)
  {
    for (size_t i = 0; i < items.size(); ++i)
    {
      if (m_view.metadata.records[i].id == *oldId)
      {
        current = items[i];
      }
    }
  }
  m_tree->setCurrentItem(current);
  m_members->clear();
  auto memberIndices = std::map<mdl::NativeVisgroupMemberId, size_t>{};
  for (size_t i = 0; i < m_view.metadata.members.size(); ++i)
  {
    memberIndices.emplace(m_view.metadata.members[i].id, i);
  }
  for (size_t i = 0; i < m_view.metadata.members.size(); ++i)
  {
    const auto& member = m_view.metadata.members[i];
    if (!member.active)
    {
      continue;
    }
    auto identity = kindText(member.kind) + " " + QString::fromStdString(member.nativeId);
    if (member.owner)
    {
      const auto owner = memberIndices.find(*member.owner);
      if (owner != memberIndices.end())
      {
        identity +=
          tr(" / owner %1")
            .arg(QString::fromStdString(m_view.metadata.members[owner->second].nativeId));
      }
    }
    auto refs = QStringList{};
    for (const auto& ref : member.memberships)
    {
      refs << QString::fromStdString(ref);
    }
    auto reason = tr("Own manual %1; auto %2; wrapper %3.")
                    .arg(
                      flagText(member.manualShown),
                      flagText(member.autoShown),
                      member.wrapperHidden ? tr("hidden") : tr("none"));
    const auto& display = m_view.members[i];
    reason += display.effectiveNativeVisibility
                ? tr(" Effective native (including inherited): %1.")
                    .arg(causes(*display.effectiveNativeVisibility))
                : tr(" Effective native: unknown (unprojected).");
    reason += display.sessionVisible ? (*display.sessionVisible ? tr(" Session shown.")
                                                                : tr(" Session hidden."))
                                     : tr(" Session unknown.");
    if (!member.editable)
    {
      reason += tr(" Protected: %1 %2")
                  .arg(
                    QString::fromStdString(member.refusalReason),
                    QString::fromStdString(member.contextRefusalReason));
    }
    auto* item =
      new QTreeWidgetItem{m_members, QStringList{identity, refs.join(", "), reason}};
    item->setData(0, Qt::UserRole, int(i));
    item->setToolTip(2, reason);
    if (currentMember == member.id)
    {
      m_members->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
    }
    if (selectedMembers.contains(member.id))
    {
      item->setSelected(true);
    }
  }
  showRows();
  if (m_pendingAction)
  {
    m_status->setText(
      tr("Stale visgroup action: review current identities and try again"));
  }
}

void NativeVisgroupEditor::showRows()
{
  ++m_rowRevision;
  const auto& snapshot = m_view.metadata;
  const auto index = recordIndex();
  const bool valid = index >= 0 && size_t(index) < snapshot.records.size();
  const bool available = snapshot.refusalReason.empty();
  const bool editable =
    available && valid && snapshot.records[size_t(index)].refusalReason.empty();
  const QSignalBlocker nameBlock{m_name};
  const QSignalBlocker parentBlock{m_parent};
  m_name->setText(
    valid ? QString::fromUtf8(snapshot.records[size_t(index)].name) : QString{});
  m_name->setModified(false);
  m_parent->clear();
  m_parent->addItem(tr("Root"));
  int parentIndex = 0;
  for (size_t i = 0; i < snapshot.records.size(); ++i)
  {
    const auto& record = snapshot.records[i];
    m_parent->addItem(
      QString::fromUtf8(record.name) + " [" + QString::fromStdString(record.nativeId)
      + "]");
    if (valid && snapshot.records[size_t(index)].parent == record.id)
    {
      parentIndex = int(i) + 1;
    }
  }
  m_parent->setCurrentIndex(parentIndex);
  m_name->setEnabled(editable);
  const auto* selectedRecord = valid ? &snapshot.records[size_t(index)] : nullptr;
  const auto colorEditable = editable && selectedRecord->colorRefusalReason.empty();
  for (size_t i = 0; i < m_color.size(); ++i)
  {
    const QSignalBlocker colorBlock{m_color[i]};
    m_color[i]->setValue(
      selectedRecord && selectedRecord->color ? (*selectedRecord->color)[i] : 255);
    m_color[i]->setEnabled(colorEditable);
  }
  m_setColor->setEnabled(colorEditable);
  m_colorState->setText(
    !selectedRecord ? tr("No visgroup selected.")
    : !selectedRecord->colorRefusalReason.empty()
      ? QString::fromStdString(selectedRecord->colorRefusalReason)
    : selectedRecord->color
      ? tr("Stored RGB; only Set RGB authors these values.")
      : tr("RGB not stored. White is a draft only; Set RGB explicitly writes it."));
  m_parent->setEnabled(available);
  m_create->setEnabled(available);
  for (auto* button :
       {m_apply,
        m_delete,
        m_show,
        m_hide,
        m_assignSelection,
        m_unassignSelection,
        m_assignMembers,
        m_unassignMembers})
  {
    button->setEnabled(editable);
  }
  m_status->setText(
    QString::fromStdString(
      !available ? snapshot.refusalReason
      : valid    ? snapshot.records[size_t(index)].refusalReason
                 : ""));

  auto treeChildren = std::map<mdl::NativeVisgroupId, std::vector<size_t>>{};
  for (size_t i = 0; i < snapshot.records.size(); ++i)
  {
    if (snapshot.records[i].parent)
    {
      treeChildren[*snapshot.records[i].parent].push_back(i);
    }
  }
  auto groupIds = std::set<std::string>{};
  auto pendingGroups = std::vector<size_t>{};
  if (valid)
  {
    pendingGroups.push_back(size_t(index));
  }
  for (size_t i = 0; i < pendingGroups.size(); ++i)
  {
    const auto& record = snapshot.records[pendingGroups[i]];
    if (!groupIds.insert(decimalKey(record.nativeId)).second)
    {
      continue;
    }
    const auto found = treeChildren.find(record.id);
    if (found != treeChildren.end())
    {
      pendingGroups.insert(
        pendingGroups.end(), found->second.begin(), found->second.end());
    }
  }
  // Derived display only; backend independently validates the full desired closure.
  auto memberChildren = std::map<mdl::NativeVisgroupMemberId, std::vector<size_t>>{};
  auto pendingMembers = std::vector<size_t>{};
  for (size_t i = 0; i < snapshot.members.size(); ++i)
  {
    const auto& member = snapshot.members[i];
    if (member.owner)
    {
      memberChildren[*member.owner].push_back(i);
    }
    if (std::ranges::any_of(member.memberships, [&](const auto& ref) {
          return groupIds.contains(decimalKey(ref));
        }))
    {
      pendingMembers.push_back(i);
    }
  }
  auto closure = std::vector<bool>(snapshot.members.size(), false);
  for (size_t i = 0; i < pendingMembers.size(); ++i)
  {
    const auto row = pendingMembers[i];
    if (closure[row])
    {
      continue;
    }
    closure[row] = true;
    const auto found = memberChildren.find(snapshot.members[row].id);
    if (found != memberChildren.end())
    {
      pendingMembers.insert(
        pendingMembers.end(), found->second.begin(), found->second.end());
    }
  }
  size_t shown = 0, hidden = 0, absent = 0, protectedCount = 0, stillHidden = 0,
         unknown = 0;
  for (size_t i = 0; i < snapshot.members.size(); ++i)
  {
    const auto& member = snapshot.members[i];
    if (!closure[i] || !member.active)
    {
      continue;
    }
    if (!member.manualShown)
    {
      ++absent;
    }
    else if (*member.manualShown)
    {
      ++shown;
    }
    else
    {
      ++hidden;
    }
    if (!member.editable)
    {
      ++protectedCount;
    }
    if (!m_view.members[i].effectiveNativeVisibility)
    {
      ++unknown;
    }
    else if (m_view.members[i].effectiveNativeVisibility->hidden())
    {
      ++stillHidden;
    }
  }
  m_summary->setText(
    tr(
      "Derived closure manual: %1 shown / %2 hidden / %3 not set; %4 protected; %5 "
      "effectively native hidden / %6 unknown. Session visibility is separate (not "
      "class/tag/view filters). Inspect member rows for own/inherited causes.")
      .arg(qulonglong(shown))
      .arg(qulonglong(hidden))
      .arg(qulonglong(absent))
      .arg(qulonglong(protectedCount))
      .arg(qulonglong(stillHidden))
      .arg(qulonglong(unknown)));

  const auto state = std::make_shared<const mdl::NativeVisgroupSnapshot>(snapshot);
  const auto bind = [&](QPushButton* button, auto build) {
    disconnect(button, nullptr, this, nullptr);
    connect(
      button,
      &QPushButton::pressed,
      this,
      [this, button, build, state, revision = m_rowRevision] {
        m_pendingButton = button;
        auto request = mdl::NativeVisgroupRequest{*state};
        if (!build(request))
        {
          m_pendingAction = {};
          return;
        }
        m_pendingAction = [this, request = std::move(request), revision] {
          if (revision != m_rowRevision)
          {
            m_status->setText(
              tr("Stale visgroup action: review current identities and try again"));
            return;
          }
          dispatch(request);
        };
      });
    connect(button, &QPushButton::clicked, this, [this, button] {
      if (m_pendingButton != button)
      {
        m_pendingAction = {};
        m_pendingButton = nullptr;
        m_status->setText(tr("Stale visgroup action: button identity changed"));
        return;
      }
      m_pendingButton = nullptr;
      auto action = std::move(m_pendingAction);
      m_pendingAction = {};
      if (action)
      {
        action();
      }
    });
  };
  const auto parent = [this, state]() -> std::optional<mdl::NativeVisgroupTarget> {
    const auto i = m_parent->currentIndex() - 1;
    return i >= 0 && size_t(i) < state->records.size()
             ? std::optional<mdl::NativeVisgroupTarget>{state->records[size_t(i)].id}
             : std::nullopt;
  };
  bind(m_create, [parent](auto& request) {
    request.records.push_back({mdl::NativeVisgroupDraftKey{1}, parent(), "New visgroup"});
    return true;
  });
  bind(m_apply, [this, index, valid, parent](auto& request) {
    if (!valid)
    {
      return false;
    }
    auto record = request.records[size_t(index)];
    const auto previousParent = record.parent;
    record.parent = parent();
    if (m_name->isModified())
    {
      record.name = m_name->text().toUtf8().toStdString();
    }
    if (record.parent != previousParent)
    {
      // Retained siblings keep their order; a moved record appends at its destination.
      request.records.erase(request.records.begin() + index);
      request.records.push_back(std::move(record));
    }
    else
    {
      request.records[size_t(index)] = std::move(record);
    }
    return true;
  });
  bind(m_setColor, [this, index, valid, state](auto& request) {
    if (!valid)
    {
      return false;
    }
    request.color = mdl::NativeVisgroupColorAction{
      state->records[size_t(index)].id,
      {m_color[0]->value(), m_color[1]->value(), m_color[2]->value()}};
    return true;
  });
  bind(m_delete, [index, valid, state, treeChildren](auto& request) {
    if (!valid)
    {
      return false;
    }
    auto deleted = std::set<mdl::NativeVisgroupId>{};
    auto pending = std::vector<size_t>{size_t(index)};
    for (size_t i = 0; i < pending.size(); ++i)
    {
      const auto id = state->records[pending[i]].id;
      if (!deleted.insert(id).second)
      {
        continue;
      }
      const auto found = treeChildren.find(id);
      if (found != treeChildren.end())
      {
        pending.insert(pending.end(), found->second.begin(), found->second.end());
      }
    }
    std::erase_if(request.records, [&](const auto& record) {
      const auto* id = std::get_if<mdl::NativeVisgroupId>(&record.key);
      return id && deleted.contains(*id);
    });
    return true;
  });
  for (const auto shownAction : {true, false})
  {
    bind(
      shownAction ? m_show : m_hide, [valid, index, state, shownAction](auto& request) {
        if (!valid)
        {
          return false;
        }
        request.manual = mdl::NativeVisgroupManualAction{
          {state->records[size_t(index)].id}, shownAction};
        return true;
      });
  }
  const auto membership = [this, index, valid, state](
                            auto& request, const bool assign, const bool selection) {
    if (!valid)
    {
      return false;
    }
    auto roots = selection ? mdl::nativeVisgroupSelection(m_document.map())
                           : std::vector<mdl::NativeVisgroupMemberId>{};
    if (!selection)
    {
      for (const auto* item : m_members->selectedItems())
      {
        roots.push_back(state->members[size_t(item->data(0, Qt::UserRole).toInt())].id);
      }
    }
    if (roots.empty())
    {
      m_status->setText(
        tr("Choose explicit member handles or a current native selection"));
      return false;
    }
    request.memberships.push_back(
      {std::move(roots), state->records[size_t(index)].id, assign});
    return true;
  };
  bind(m_assignSelection, [membership](auto& request) {
    return membership(request, true, true);
  });
  bind(m_unassignSelection, [membership](auto& request) {
    return membership(request, false, true);
  });
  bind(m_assignMembers, [membership](auto& request) {
    return membership(request, true, false);
  });
  bind(m_unassignMembers, [membership](auto& request) {
    return membership(request, false, false);
  });
}

void NativeVisgroupEditor::dispatch(const mdl::NativeVisgroupRequest& request)
{
  const auto result = mdl::applyNativeVisgroups(m_document.map(), request);
  if (result.status == mdl::NativeVisgroupApplyStatus::Refused)
  {
    m_status->setText(QString::fromStdString(result.reason));
  }
  else
  {
    refresh();
  }
}
} // namespace tb::ui
