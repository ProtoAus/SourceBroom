/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/NativeCameraEditor.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_NativeCameras.h"
#include "mdl/SourceEditorMetadata.h"
#include "mdl/WorldNode.h"
#include "ui/MapDocument.h"

#include <locale>
#include <sstream>
#include <utility>
namespace tb::ui
{
namespace
{
QString vectorText(const vm::vec3d& point)
{
  return QString::number(point.x(), 'g', 17) + " " + QString::number(point.y(), 'g', 17)
         + " " + QString::number(point.z(), 'g', 17);
}
std::optional<vm::vec3d> parseVector(const QString& text)
{
  auto input = std::istringstream{text.toStdString()};
  input.imbue(std::locale::classic());
  auto point = vm::vec3d{};
  for (size_t i = 0; i < 3; ++i)
  {
    if (!(input >> point[i]))
    {
      return std::nullopt;
    }
  }
  input >> std::ws;
  return input.eof() ? std::optional<vm::vec3d>{point} : std::nullopt;
}
} // namespace
NativeCameraEditor::NativeCameraEditor(
  MapDocument& document, QWidget* parent, NativeCameraCapture capture)
  : QWidget{parent}
  , m_document{document}
  , m_capture{std::move(capture)}
{
  setObjectName("nativeCameraEditor");
  auto* layout = new QVBoxLayout{this};
  auto* warning = new QLabel{tr(
    "LIVE native cameras (including UNSAVED changes). Read the active 3D view into "
    "the eye/look draft, then explicitly Add/Apply. No viewport movement or "
    "projection/FOV import.")};
  warning->setWordWrap(true);
  layout->addWidget(warning);
  auto* form = new QFormLayout{};
  m_records = new QComboBox{};
  m_records->setObjectName("nativeCameraRecords");
  form->addRow(tr("Camera record"), m_records);
  m_eye = new QLineEdit{};
  m_eye->setObjectName("nativeCameraEye");
  form->addRow(tr("Eye (x y z)"), m_eye);
  m_look = new QLineEdit{};
  m_look->setObjectName("nativeCameraLook");
  form->addRow(tr("Look target (x y z)"), m_look);
  m_active = new QComboBox{};
  m_active->setObjectName("nativeCameraActive");
  form->addRow(tr("Stored active camera"), m_active);
  layout->addLayout(form);
  const auto button = [&](const char* name, const QString& text) {
    auto* result = new QPushButton{text};
    result->setObjectName(name);
    layout->addWidget(result);
    return result;
  };
  m_captureView = button("nativeCameraCapture", tr("Read current 3D view into draft"));
  m_add = button("nativeCameraAdd", tr("Add camera from displayed eye/look"));
  m_applyPose =
    button("nativeCameraApply", tr("Apply edited eye/look to selected camera"));
  m_delete = button("nativeCameraDelete", tr("Delete selected camera"));
  m_applyActive = button("nativeCameraApplyActive", tr("Apply active-camera choice"));
  m_remove = button("nativeCameraRemove", tr("Remove camera controller and its records"));
  m_status = new QLabel{};
  m_status->setObjectName("nativeCameraStatus");
  m_status->setWordWrap(true);
  layout->addWidget(m_status);
  connect(m_captureView, &QPushButton::pressed, this, [this] {
    m_pendingAction = {};
    m_pendingButton = nullptr;
  });
  connect(m_captureView, &QPushButton::clicked, this, [this] {
    m_pendingAction = {};
    m_pendingButton = nullptr;
    if (!m_capture || !m_snapshot.refusalReason.empty())
    {
      return;
    }
    const auto revision = m_rowRevision;
    const auto draft = m_capture();
    if (revision != m_rowRevision)
    {
      m_status->setText(tr("Camera state changed; review current fields and try again"));
      return;
    }
    if (!draft || !mdl::SourceEditorCamera{0, draft->eye, draft->look}.navigable())
    {
      m_status->setText(tr("Activate a valid 3D viewport, then read its view again"));
      return;
    }
    m_eye->setText(vectorText(draft->eye));
    m_look->setText(vectorText(draft->look));
    m_eye->setModified(true);
    m_look->setModified(true);
    m_status->setText(
      tr("Eye/look draft only. Explicitly Add or Apply to write a camera."));
  });
  connect(m_records, &QComboBox::currentIndexChanged, this, [this] { showRow(); });
  connect(m_active, &QComboBox::currentIndexChanged, this, [this](int index) {
    m_applyActive->setEnabled(m_snapshot.refusalReason.empty() && index >= 0);
  });
  m_connections += m_document.documentWasLoadedNotifier.connect([this] { refresh(); });
  m_connections += m_document.documentDidChangeNotifier.connect([this] { refresh(); });
  refresh();
}
void NativeCameraEditor::refresh()
{
  m_snapshot = mdl::nativeCameras(m_document.map());
  setVisible(m_document.map().worldNode().mapFormat() == mdl::MapFormat::SourceVMF);
  const auto previous = m_records->currentIndex();
  const QSignalBlocker recordBlock{m_records};
  const QSignalBlocker activeBlock{m_active};
  m_records->clear();
  m_active->clear();
  m_active->addItems({tr("Not stored"), tr("No active camera (-1)")});
  for (size_t i = 0; i < m_snapshot.records.size(); ++i)
  {
    const auto label = tr("Camera %1").arg(i + 1);
    m_records->addItem(label);
    m_active->addItem(label);
  }
  m_records->setCurrentIndex(
    previous >= 0 && previous < m_records->count() ? previous : 0);
  showRow();
}
void NativeCameraEditor::showRow()
{
  ++m_rowRevision;
  const auto snapshot = m_snapshot;
  const auto row = m_records->currentIndex();
  const bool valid = row >= 0 && size_t(row) < snapshot.records.size();
  const bool available = snapshot.refusalReason.empty();
  const QSignalBlocker activeBlock{m_active};
  m_active->setCurrentIndex(
    snapshot.active.kind == mdl::NativeCameraActiveKind::None ? 1 : 0);
  if (snapshot.active.kind == mdl::NativeCameraActiveKind::Camera)
  {
    for (size_t i = 0; i < snapshot.records.size(); ++i)
    {
      if (snapshot.records[i].id == snapshot.active.camera)
      {
        m_active->setCurrentIndex(int(i) + 2);
      }
    }
  }
  m_eye->setText(valid ? vectorText(snapshot.records[size_t(row)].eye) : "0 0 0");
  m_look->setText(valid ? vectorText(snapshot.records[size_t(row)].look) : "1 0 0");
  m_eye->setModified(false);
  m_look->setModified(false);
  m_eye->setEnabled(available);
  m_look->setEnabled(available);
  m_active->setEnabled(available);
  m_captureView->setEnabled(available && bool(m_capture));
  m_add->setEnabled(
    available && snapshot.records.size() < mdl::NativeCameras::MaxRecords);
  m_applyPose->setEnabled(available && valid);
  m_delete->setEnabled(available && valid);
  m_applyActive->setEnabled(available && m_active->currentIndex() >= 0);
  m_remove->setEnabled(available && snapshot.present);
  m_status->setText(QString::fromStdString(snapshot.refusalReason));
  // Freeze scope, row and field intent on press. A later refresh cannot retarget release.
  const auto bind = [&](QPushButton* button, auto makeRequest) {
    disconnect(button, nullptr, this, nullptr);
    const auto generation = m_rowRevision;
    connect(
      button,
      &QPushButton::pressed,
      this,
      [this, snapshot, button, makeRequest, generation] {
        m_pendingAction = {};
        m_pendingButton = button;
        auto request = mdl::NativeCameraRequest{snapshot};
        if (!makeRequest(request))
        {
          return;
        }
        m_pendingAction = [this, request = std::move(request), generation] {
          if (generation != m_rowRevision)
          {
            m_status->setText(
              tr("Stale camera row: review current fields and try again"));
            return;
          }
          dispatch(request);
        };
      });
    connect(button, &QPushButton::clicked, this, [this, button] {
      if (m_pendingButton != button)
      {
        return;
      }
      auto action = std::move(m_pendingAction);
      m_pendingAction = {};
      m_pendingButton = nullptr;
      if (action)
      {
        action();
      }
    });
  };
  bind(m_add, [this](auto& request) {
    const auto eye = parseVector(m_eye->text());
    const auto look = parseVector(m_look->text());
    if (!eye || !look)
    {
      m_status->setText(tr("Eye/look require XYZ tuples"));
      return false;
    }
    request.present = true;
    request.records.push_back({{}, *eye, *look});
    return true;
  });
  bind(m_applyPose, [this, valid, row](auto& request) {
    if (!valid)
    {
      return false;
    }
    auto& record = request.records[size_t(row)];
    if (m_eye->isModified())
    {
      const auto value = parseVector(m_eye->text());
      if (!value)
      {
        m_status->setText(tr("Invalid eye XYZ tuple"));
        return false;
      }
      record.eye = *value;
    }
    if (m_look->isModified())
    {
      const auto value = parseVector(m_look->text());
      if (!value)
      {
        m_status->setText(tr("Invalid look XYZ tuple"));
        return false;
      }
      record.look = *value;
    }
    return true;
  });
  bind(m_delete, [valid, row](auto& request) {
    if (!valid)
    {
      return false;
    }
    const auto id = request.records[size_t(row)].id;
    if (
      request.active.kind == mdl::NativeCameraActiveKind::Camera
      && request.active.camera == id)
    {
      request.active = {mdl::NativeCameraActiveKind::None, {}};
    }
    request.records.erase(request.records.begin() + row);
    return true;
  });
  bind(m_applyActive, [this](auto& request) {
    const auto index = m_active->currentIndex();
    if (index < 0 || size_t(index) > request.records.size() + 1)
    {
      return false;
    }
    request.active = {};
    if (index != 0)
    {
      request.present = true;
      request.active =
        index == 1
          ? mdl::NativeCameraActivation{mdl::NativeCameraActiveKind::None, {}}
          : mdl::NativeCameraActivation{
              mdl::NativeCameraActiveKind::Camera, request.records[size_t(index - 2)].id};
    }
    return true;
  });
  bind(m_remove, [](auto& request) {
    request.present = false;
    request.records.clear();
    request.active = {};
    return true;
  });
}
void NativeCameraEditor::dispatch(const mdl::NativeCameraRequest& request)
{
  const auto result = mdl::applyNativeCameras(m_document.map(), request);
  if (result.status == mdl::NativeCameraApplyStatus::Refused)
  {
    m_status->setText(QString::fromStdString(result.reason));
  }
  else
  {
    refresh();
  }
}
} // namespace tb::ui
