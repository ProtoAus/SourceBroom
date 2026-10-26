/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/NativeCordonEditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_NativeCordons.h"
#include "mdl/WorldNode.h"
#include "ui/MapDocument.h"

#include <locale>
#include <sstream>

namespace tb::ui
{
namespace
{
QString vectorText(const vm::vec3d& v)
{
  return QString::number(v.x(), 'g', 17) + " " + QString::number(v.y(), 'g', 17) + " "
         + QString::number(v.z(), 'g', 17);
}
std::optional<vm::vec3d> parseVector(const QString& text)
{
  auto stream = std::istringstream{text.toStdString()};
  stream.imbue(std::locale::classic());
  double x, y, z;
  if (!(stream >> x >> y >> z))
  {
    return std::nullopt;
  }
  stream >> std::ws;
  if (!stream.eof())
  {
    return std::nullopt;
  }
  return vm::vec3d{x, y, z};
}
int flagIndex(const std::optional<bool> value)
{
  return value ? (*value ? 2 : 1) : 0;
}
std::optional<bool> flagValue(const QComboBox* box)
{
  return box->currentIndex() == 0 ? std::nullopt
                                  : std::optional<bool>{box->currentIndex() == 2};
}
} // namespace

NativeCordonEditor::NativeCordonEditor(MapDocument& document, QWidget* parent)
  : QWidget{parent}
  , m_document{document}
{
  auto* layout = new QVBoxLayout{this};
  auto* warning = new QLabel{tr(
    "LIVE native cordons (including UNSAVED changes). These metadata bounds do not clip "
    "this viewport or guarantee external compiler-region behavior.")};
  warning->setWordWrap(true);
  layout->addWidget(warning);
  m_form = new QLabel{};
  m_form->setObjectName("cordonForm");
  layout->addWidget(m_form);
  auto* form = new QFormLayout{};
  const auto combo = [&](const char* name, const QString& label, bool flag) {
    auto* widget = new QComboBox{};
    widget->setObjectName(name);
    if (flag)
    {
      widget->addItems({tr("Not set"), tr("Inactive"), tr("Active")});
    }
    form->addRow(label, widget);
    return widget;
  };
  m_records = combo("cordonRecords", tr("Cordon"), false);
  m_boxes = combo("cordonBoxes", tr("Box"), false);
  m_global = combo("cordonGlobal", tr("Modern controller activation"), true);
  m_active = combo("cordonActive", tr("Cordon activation"), true);
  const auto edit = [&](const char* name, const QString& label) {
    auto* widget = new QLineEdit{};
    widget->setObjectName(name);
    form->addRow(label, widget);
    return widget;
  };
  m_name = edit("cordonName", tr("Name"));
  m_mins = edit("cordonMins", tr("Ordered min (x y z)"));
  m_maxs = edit("cordonMaxs", tr("Ordered max (x y z)"));
  layout->addLayout(form);
  const auto button = [&](const char* name, const QString& label) {
    auto* widget = new QPushButton{label};
    widget->setObjectName(name);
    layout->addWidget(widget);
    return widget;
  };
  m_addRecord =
    button("cordonAddRecord", tr("Add cordon (inactive, initial box -64 to 64)"));
  m_addBox = button("cordonAddBox", tr("Add box (-64 to 64)"));
  m_apply = button("cordonApply", tr("Apply fields atomically"));
  m_fit = button("cordonFit", tr("Fit box to current selection"));
  m_deleteBox = button("cordonDeleteBox", tr("Delete box"));
  m_deleteRecord = button("cordonDeleteRecord", tr("Delete cordon"));
  m_clear = button("cordonClear", tr("Remove controller"));
  m_status = new QLabel{};
  m_status->setObjectName("cordonStatus");
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
  connect(m_records, &QComboBox::currentIndexChanged, this, [this] {
    const QSignalBlocker blocker{m_boxes};
    m_boxes->clear();
    showRows();
  });
  connect(m_boxes, &QComboBox::currentIndexChanged, this, [this] { showRows(); });
  m_connections += m_document.documentWasLoadedNotifier.connect([this] { refresh(); });
  m_connections += m_document.documentDidChangeNotifier.connect([this] { refresh(); });
  refresh();
}

void NativeCordonEditor::refresh()
{
  m_snapshot = mdl::nativeCordons(m_document.map());
  setVisible(m_document.map().worldNode().mapFormat() == mdl::MapFormat::SourceVMF);
  const auto oldRecord = m_records->currentIndex();
  const auto oldBox = m_boxes->currentIndex();
  const QSignalBlocker recordBlock{m_records};
  const QSignalBlocker boxBlock{m_boxes};
  m_records->clear();
  for (const auto& record : m_snapshot.records)
  {
    m_records->addItem(
      record.name ? QString::fromUtf8(*record.name) : tr("(name not set)"));
  }
  m_records->setCurrentIndex(
    oldRecord >= 0 && oldRecord < m_records->count() ? oldRecord : 0);
  m_boxes->clear();
  showRows();
  m_boxes->setCurrentIndex(oldBox >= 0 && oldBox < m_boxes->count() ? oldBox : 0);
  showRows();
}

void NativeCordonEditor::showRows()
{
  ++m_rowRevision;
  const auto snapshot = m_snapshot;
  const auto r = m_records->currentIndex();
  const bool recordValid = r >= 0 && size_t(r) < snapshot.records.size();
  if (m_boxes->count() == 0 && recordValid)
  {
    const QSignalBlocker blocker{m_boxes};
    for (size_t i = 0; i < snapshot.records[size_t(r)].boxes.size(); ++i)
    {
      m_boxes->addItem(tr("Box %1").arg(i + 1));
    }
  }
  const auto b = m_boxes->currentIndex();
  const bool boxValid =
    recordValid && b >= 0 && size_t(b) < snapshot.records[size_t(r)].boxes.size();
  const bool modern = snapshot.form == mdl::NativeCordonForm::Modern;
  const bool legacy = snapshot.form == mdl::NativeCordonForm::Legacy;
  const bool available = snapshot.refusalReason.empty();
  m_form->setText(
    legacy   ? tr("Legacy singleton: bounds/activation/removal only; no conversion.")
    : modern ? tr("Modern controller")
             : tr("No controller"));
  m_status->setText(QString::fromStdString(snapshot.refusalReason));
  const QSignalBlocker globalBlock{m_global};
  const QSignalBlocker activeBlock{m_active};
  const QSignalBlocker nameBlock{m_name};
  const QSignalBlocker minsBlock{m_mins};
  const QSignalBlocker maxsBlock{m_maxs};
  m_global->setCurrentIndex(flagIndex(snapshot.active));
  m_active->setCurrentIndex(
    recordValid ? flagIndex(snapshot.records[size_t(r)].active) : 0);
  m_name->setText(
    recordValid && snapshot.records[size_t(r)].name
      ? QString::fromUtf8(*snapshot.records[size_t(r)].name)
      : QString{});
  m_name->setModified(false);
  m_mins->setText(
    boxValid ? vectorText(snapshot.records[size_t(r)].boxes[size_t(b)].mins) : QString{});
  m_maxs->setText(
    boxValid ? vectorText(snapshot.records[size_t(r)].boxes[size_t(b)].maxs) : QString{});
  m_mins->setModified(false);
  m_maxs->setModified(false);
  m_global->setEnabled(available && modern);
  m_active->setEnabled(available && recordValid);
  m_name->setEnabled(available && modern && recordValid);
  m_mins->setEnabled(available && boxValid);
  m_maxs->setEnabled(available && boxValid);
  m_addRecord->setEnabled(available && !legacy);
  m_addBox->setEnabled(available && modern && recordValid);
  m_apply->setEnabled(available && (modern || legacy));
  m_fit->setEnabled(available && boxValid);
  m_deleteBox->setEnabled(
    available && modern && recordValid && snapshot.records[size_t(r)].boxes.size() > 1);
  m_deleteRecord->setEnabled(available && recordValid);
  m_clear->setEnabled(available && snapshot.form != mdl::NativeCordonForm::None);

  // Press captures the owning scope/revision/identities and UI row generation.
  // Release after refresh or row switching refuses rather than reusing new fields.
  const auto bind = [&](QPushButton* button, auto action) {
    disconnect(button, nullptr, this, nullptr);
    const auto rowRevision = m_rowRevision;
    connect(button, &QPushButton::pressed, this, [this, snapshot, action, rowRevision] {
      m_pendingAction = [this, snapshot, action, rowRevision] {
        if (rowRevision != m_rowRevision)
        {
          m_status->setText(tr("Stale cordon row: review current fields and try again"));
          return;
        }
        auto request = mdl::NativeCordonRequest{snapshot};
        if (action(request))
        {
          dispatch(request);
        }
      };
    });
    connect(button, &QPushButton::clicked, this, [this] {
      auto action = std::move(m_pendingAction);
      m_pendingAction = {};
      if (action)
      {
        action();
      }
    });
  };
  bind(m_addRecord, [](auto& request) {
    if (request.form == mdl::NativeCordonForm::None)
    {
      request.form = mdl::NativeCordonForm::Modern;
      request.active = false;
    }
    request.records.push_back(
      {{}, "New cordon", false, {{{}, {-64, -64, -64}, {64, 64, 64}}}});
    return true;
  });
  bind(m_addBox, [r](auto& request) {
    if (r < 0 || size_t(r) >= request.records.size())
    {
      return false;
    }
    request.records[size_t(r)].boxes.push_back({{}, {-64, -64, -64}, {64, 64, 64}});
    return true;
  });
  const auto clear = [](auto& request) {
    request.form = mdl::NativeCordonForm::None;
    request.active.reset();
    request.records.clear();
    return true;
  };
  bind(m_clear, clear);
  bind(m_deleteRecord, [r, clear](auto& request) {
    if (r < 0 || size_t(r) >= request.records.size())
    {
      return false;
    }
    request.records.erase(request.records.begin() + r);
    if (request.records.empty())
    {
      clear(request);
    }
    return true;
  });
  bind(m_deleteBox, [r, b, boxValid](auto& request) {
    if (!boxValid)
    {
      return false;
    }
    auto& boxes = request.records[size_t(r)].boxes;
    if (boxes.size() <= 1)
    {
      return false;
    }
    boxes.erase(boxes.begin() + b);
    return true;
  });
  bind(m_apply, [this, r, b, modern, recordValid, boxValid](auto& request) {
    if (modern)
    {
      request.active = flagValue(m_global);
    }
    if (recordValid)
    {
      auto& record = request.records[size_t(r)];
      record.active = flagValue(m_active);
      if (modern && m_name->isModified())
      {
        record.name = m_name->text().toUtf8().toStdString();
      }
      if (boxValid)
      {
        auto& box = record.boxes[size_t(b)];
        if (m_mins->isModified())
        {
          const auto value = parseVector(m_mins->text());
          if (!value)
          {
            m_status->setText(tr("Invalid ordered minimum"));
            return false;
          }
          box.mins = *value;
        }
        if (m_maxs->isModified())
        {
          const auto value = parseVector(m_maxs->text());
          if (!value)
          {
            m_status->setText(tr("Invalid ordered maximum"));
            return false;
          }
          box.maxs = *value;
        }
      }
    }
    return true;
  });
  bind(m_fit, [this, r, b, boxValid](auto& request) {
    const auto bounds = m_document.map().selectionBounds();
    if (!boxValid || !bounds)
    {
      m_status->setText(tr("Fit requires valid nondegenerate selected bounds"));
      return false;
    }
    request.records[size_t(r)].boxes[size_t(b)].mins = bounds->min;
    request.records[size_t(r)].boxes[size_t(b)].maxs = bounds->max;
    return true;
  });
}
void NativeCordonEditor::dispatch(const mdl::NativeCordonRequest& request)
{
  const auto result = mdl::applyNativeCordons(m_document.map(), request);
  if (result.status == mdl::NativeCordonApplyStatus::Refused)
  {
    m_status->setText(QString::fromStdString(result.reason));
  }
  else
  {
    refresh();
  }
}
} // namespace tb::ui
