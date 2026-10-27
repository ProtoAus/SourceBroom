/*
 Copyright (C) 2026 SourceBroom contributors
 SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ui/SourceToolsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringList>
#include <QTabWidget>
#include <QVBoxLayout>

#include "ui/QPathUtils.h"
#include "ui/SourceDocumentReader.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <utility>

namespace tb::ui
{
namespace
{
QString text(const std::string& value)
{
  return QString::fromStdString(value);
}
QString id(const std::optional<std::string>& value)
{
  return value ? text(*value) : QString{"unknown"};
}
QString flag(const std::optional<bool> value)
{
  return value ? (*value ? QString{"yes"} : QString{"no"}) : QString{"unknown"};
}
QString vectorText(const vm::vec3d& value)
{
  return QString{"(%1 %2 %3)"}.arg(value.x()).arg(value.y()).arg(value.z());
}
QString issueName(const mdl::SourceDependencyPackIssueKind kind)
{
  switch (kind)
  {
  case mdl::SourceDependencyPackIssueKind::Excluded:
    return "EXCLUDED";
  case mdl::SourceDependencyPackIssueKind::Missing:
    return "MISSING";
  case mdl::SourceDependencyPackIssueKind::Unresolved:
    return "UNRESOLVED";
  case mdl::SourceDependencyPackIssueKind::Budget:
    return "BUDGET";
  case mdl::SourceDependencyPackIssueKind::Error:
    return "ERROR";
  }
  return "UNKNOWN";
}
} // namespace

SourceToolsDialog::SourceToolsDialog(
  QWidget* parent, QString savedVmf, const SourceBspTarget target)
  : QDialog{parent}
  , m_savedVmf{std::move(savedVmf)}
  , m_target{target}
{
  setWindowTitle(tr("Native Source tools — saved VMF snapshot"));
  resize(850, 650);
  auto* layout = new QVBoxLayout{this};
  auto* path = new QLineEdit{m_savedVmf, this};
  path->setReadOnly(true);
  layout->addWidget(path);
  layout->addWidget(new QLabel{
    tr(
      "Reads the saved VMF only. Save editor changes first. Originals are never "
      "written."),
    this});
  auto* tabs = new QTabWidget{this};
  layout->addWidget(tabs, 1);
  auto* metadataPage = new QWidget{tabs};
  auto* metadataLayout = new QVBoxLayout{metadataPage};
  metadataLayout->addWidget(new QLabel{
    tr(
      "Groups, visgroups, hidden state, cameras and cordons are read-only.\n"
      "Membership is not scene parenting. SourceBroom does not apply cordon clipping;\n"
      "external tool behavior follows the unchanged native VMF and must be checked."),
    metadataPage});
  m_metadataText = new QPlainTextEdit{metadataPage};
  m_metadataText->setObjectName("sourceMetadataText");
  m_metadataText->setReadOnly(true);
  metadataLayout->addWidget(m_metadataText, 1);
  auto* cameraRow = new QHBoxLayout{};
  m_cameras = new QComboBox{metadataPage};
  m_cameras->setObjectName("sourceMetadataCameras");
  m_goCamera = new QPushButton{tr("Go to camera eye"), metadataPage};
  m_goCamera->setObjectName("sourceMetadataGoCamera");
  m_goCameraView = new QPushButton{tr("Go to camera view"), metadataPage};
  m_goCameraView->setObjectName("sourceMetadataGoCameraView");
  m_goCameraView->setToolTip(tr(
    "Uses the saved eye and look direction; keeps projection and zoom. No VMF changes."));
  cameraRow->addWidget(m_cameras, 1);
  cameraRow->addWidget(m_goCamera);
  cameraRow->addWidget(m_goCameraView);
  metadataLayout->addLayout(cameraRow);
  tabs->addTab(metadataPage, tr("Native metadata (read-only)"));

  auto* packPage = new QWidget{tabs};
  auto* packLayout = new QVBoxLayout{packPage};
  auto* policy = new QLabel{
    tr(
      "Custom-content distribution ZIP, not automatic BSP embedding.\n"
      "Select a CUSTOM-ONLY content root using game-relative folders (materials/, "
      "models/, sound/).\n"
      "Never select an installed game or Steam library. No mounted/VPK stock content is "
      "resolved.\n"
      "Discovery is incomplete: scripts and runtime/branch-specific dependencies need "
      "manual review.\n"
      "A written ZIP is NOT proof of licence rights or a self-contained map."),
    packPage};
  policy->setWordWrap(true);
  packLayout->addWidget(policy);
  auto* form = new QFormLayout{};
  packLayout->addLayout(form);
  auto* rootRow = new QWidget{packPage};
  auto* rootLayout = new QHBoxLayout{rootRow};
  rootLayout->setContentsMargins(0, 0, 0, 0);
  m_customRoot = new QLineEdit{rootRow};
  m_customRoot->setObjectName("sourcePackRoot");
  auto* browseRoot = new QPushButton{tr("Choose custom root…"), rootRow};
  rootLayout->addWidget(m_customRoot, 1);
  rootLayout->addWidget(browseRoot);
  form->addRow(tr("Explicit custom-only root"), rootRow);
  auto* bspRow = new QWidget{packPage};
  auto* bspLayout = new QHBoxLayout{bspRow};
  bspLayout->setContentsMargins(0, 0, 0, 0);
  m_bsp = new QLineEdit{bspRow};
  m_bsp->setObjectName("sourcePackBsp");
  auto* browseBsp = new QPushButton{tr("Choose BSP…"), bspRow};
  bspLayout->addWidget(m_bsp, 1);
  bspLayout->addWidget(browseBsp);
  form->addRow(tr("Optional compiled BSP (read-only)"), bspRow);
  m_rights = new QCheckBox{
    tr(
      "I selected custom-only content and have redistribution rights for all included "
      "files."),
    packPage};
  m_rights->setObjectName("sourcePackRights");
  packLayout->addWidget(m_rights);
  m_review = new QPushButton{tr("Discover and review exact plan"), packPage};
  m_review->setObjectName("sourcePackReview");
  packLayout->addWidget(m_review);
  m_packText = new QPlainTextEdit{packPage};
  m_packText->setReadOnly(true);
  m_packText->setObjectName("sourcePackReport");
  packLayout->addWidget(m_packText, 1);
  m_approval = new QCheckBox{
    tr("I reviewed this exact included/excluded/missing plan and approve writing it."),
    packPage};
  m_approval->setObjectName("sourcePackApproval");
  packLayout->addWidget(m_approval);
  m_export = new QPushButton{tr("Write NEW distribution ZIP…"), packPage};
  m_export->setObjectName("sourcePackExport");
  packLayout->addWidget(m_export);
  tabs->addTab(packPage, tr("Custom dependency package"));
  auto* buttons = new QHBoxLayout{};
  auto* reload = new QPushButton{tr("Reload saved snapshot"), this};
  auto* close = new QPushButton{tr("Close"), this};
  buttons->addWidget(reload);
  buttons->addStretch();
  buttons->addWidget(close);
  layout->addLayout(buttons);
  connect(close, &QPushButton::clicked, this, &QDialog::close);
  connect(reload, &QPushButton::clicked, this, &SourceToolsDialog::reloadDocument);
  connect(
    m_cameras,
    &QComboBox::currentIndexChanged,
    this,
    &SourceToolsDialog::updateCameraButton);
  connect(m_goCamera, &QPushButton::clicked, this, &SourceToolsDialog::navigateCamera);
  connect(
    m_goCameraView, &QPushButton::clicked, this, &SourceToolsDialog::navigateCameraView);
  connect(
    m_customRoot, &QLineEdit::textChanged, this, &SourceToolsDialog::invalidatePlan);
  connect(m_bsp, &QLineEdit::textChanged, this, &SourceToolsDialog::invalidatePlan);
  connect(m_rights, &QCheckBox::toggled, this, &SourceToolsDialog::invalidatePlan);
  connect(m_approval, &QCheckBox::toggled, this, &SourceToolsDialog::updateExportEnabled);
  connect(m_review, &QPushButton::clicked, this, &SourceToolsDialog::createPlan);
  connect(m_export, &QPushButton::clicked, this, &SourceToolsDialog::exportPlan);
  connect(browseRoot, &QPushButton::clicked, this, [this] {
    const auto selected =
      QFileDialog::getExistingDirectory(this, tr("Custom-only content root"));
    if (!selected.isEmpty())
    {
      m_customRoot->setText(selected);
    }
  });
  connect(browseBsp, &QPushButton::clicked, this, [this] {
    const auto selected = QFileDialog::getOpenFileName(
      this, tr("Explicit compiled BSP"), {}, tr("Source BSP (*.bsp)"));
    if (!selected.isEmpty())
    {
      m_bsp->setText(selected);
    }
  });
  reloadDocument();
}

void SourceToolsDialog::reloadDocument()
{
  auto error = QString{};
  m_document = readSavedSourceDocument(m_savedVmf, error);
  m_metadata = {};
  m_cameras->clear();
  auto rows = QStringList{};
  if (!m_document)
  {
    rows << error;
  }
  else
  {
    m_metadata = mdl::SourceEditorMetadata::read(*m_document);
    rows << tr("READ-ONLY native snapshot; unknown data remains opaque.");
    rows << tr("Groups: %1; visgroups: %2; memberships: %3; cameras: %4; cordons: %5")
              .arg(m_metadata.groups.size())
              .arg(m_metadata.visgroups.size())
              .arg(m_metadata.memberships.size())
              .arg(m_metadata.cameras.size())
              .arg(m_metadata.cordons.size());
    const auto append = [&](const QString& row) {
      if (rows.size() < 5000)
      {
        rows << row;
      }
    };
    for (const auto& group : m_metadata.groups)
    {
      append(
        QString{"Group entry %1: id %2; parent id %3"}
          .arg(group.entry)
          .arg(id(group.nativeId), id(group.parentId)));
    }
    for (const auto& group : m_metadata.visgroups)
    {
      append(
        QString{"Visgroup entry %1: id %2; name %3; parent entry %4"}
          .arg(group.entry)
          .arg(
            id(group.nativeId),
            text(group.name),
            group.parent ? QString::number(*group.parent) : QString{"none"}));
    }
    for (const auto& member : m_metadata.memberships)
    {
      auto visgroups = QStringList{};
      for (const auto& value : member.visgroupIds)
      {
        visgroups << text(value);
      }
      append(
        QString{"Member entry %1: id %2; group id %3; visgroups [%4]; hidden container "
                "%5; shown %6; auto shown %7"}
          .arg(member.entry)
          .arg(
            id(member.nativeId),
            id(member.groupId),
            visgroups.join(", "),
            member.hiddenByContainer ? QString{"yes"} : QString{"no"},
            flag(member.visgroupShown),
            flag(member.visgroupAutoShown)));
    }
    for (size_t i = 0; i < m_metadata.cameras.size(); ++i)
    {
      const auto& camera = m_metadata.cameras[i];
      append(
        QString{"Camera entry %1: eye %2; look %3; active %4; navigable %5"}
          .arg(camera.entry)
          .arg(
            camera.eye ? vectorText(*camera.eye) : QString{"invalid"},
            camera.look ? vectorText(*camera.look) : QString{"invalid"},
            m_metadata.activeCamera == camera.entry ? QString{"yes"} : QString{"no"},
            camera.navigable() ? QString{"yes"} : QString{"no"}));
      m_cameras->addItem(tr("Camera %1 (entry %2)").arg(i + 1).arg(camera.entry));
    }
    append("Global cordons active: " + flag(m_metadata.cordonsActive));
    for (const auto& cordon : m_metadata.cordons)
    {
      append(
        QString{"Cordon entry %1: %2; active %3; complete bounds valid %4"}
          .arg(cordon.entry)
          .arg(
            text(cordon.name),
            flag(cordon.active),
            cordon.boundsValid ? QString{"yes"} : QString{"no"}));
      for (const auto& box : cordon.boxes)
      {
        append("  " + vectorText(box.min) + " .. " + vectorText(box.max));
      }
    }
    for (const auto& diagnostic : m_metadata.diagnostics)
    {
      append(
        QString{"DIAGNOSTIC entry %1: %2"}
          .arg(diagnostic.entry)
          .arg(text(diagnostic.message)));
    }
    if (rows.size() >= 5000)
    {
      rows << "Display capped at 5000 rows; snapshot and native bytes are unchanged. Not "
              "all rows/diagnostics are displayed.";
    }
  }
  m_metadataText->setPlainText(rows.join('\n'));
  updateCameraButton();
  invalidatePlan();
}

std::optional<QVector3D> SourceToolsDialog::cameraEye() const
{
  const auto index = m_cameras->currentIndex();
  if (index < 0 || static_cast<size_t>(index) >= m_metadata.cameras.size())
  {
    return std::nullopt;
  }
  const auto& camera = m_metadata.cameras[static_cast<size_t>(index)];
  if (!camera.navigable())
  {
    return std::nullopt;
  }
  const auto& eye = *camera.eye;
  // A preview navigation guard, NOT a Source branch/world-bound assertion.
  for (size_t axis = 0; axis < 3; ++axis)
  {
    if (!std::isfinite(eye[axis]) || std::abs(eye[axis]) > 1'000'000.0)
    {
      return std::nullopt;
    }
  }
  return QVector3D{
    static_cast<float>(eye.x()),
    static_cast<float>(eye.y()),
    static_cast<float>(eye.z())};
}
std::optional<QVector3D> SourceToolsDialog::cameraViewDirection() const
{
  if (!cameraEye())
  {
    return std::nullopt;
  }
  const auto& camera = m_metadata.cameras[static_cast<size_t>(m_cameras->currentIndex())];
  const auto delta = *camera.look - *camera.eye;
  // Normalize before converting coordinates to float: nearby large targets would
  // collapse.
  const auto scale =
    std::max({std::abs(delta.x()), std::abs(delta.y()), std::abs(delta.z())});
  if (!std::isfinite(scale) || scale <= 0.0)
  {
    return std::nullopt;
  }
  const auto scaled = delta / scale;
  const auto direction = scaled / vm::length(scaled);
  return QVector3D{float(direction.x()), float(direction.y()), float(direction.z())}
    .normalized();
}

void SourceToolsDialog::updateCameraButton()
{
  m_goCamera->setEnabled(cameraEye().has_value());
  m_goCameraView->setEnabled(cameraViewDirection().has_value());
  m_goCamera->setToolTip(tr(
    "Moves to the eye only; preserves the current view direction. Unsafe/degenerate "
    "cameras and eyes beyond ±1,000,000 preview units are not navigation targets."));
}
void SourceToolsDialog::navigateCamera()
{
  if (const auto eye = cameraEye())
  {
    emit navigateToPosition(*eye);
  }
}

void SourceToolsDialog::navigateCameraView()
{
  const auto eye = cameraEye();
  const auto direction = cameraViewDirection();
  if (eye && direction)
  {
    emit navigateToCameraView(*eye, *direction);
  }
}

void SourceToolsDialog::invalidatePlan()
{
  m_plan.reset();
  m_approval->setChecked(false);
  m_approval->setEnabled(false);
  m_export->setEnabled(false);
  m_review->setEnabled(
    m_document.has_value() && m_rights->isChecked()
    && !m_customRoot->text().trimmed().isEmpty());
  m_packText->setPlainText(tr(
    "No reviewed plan. Choose custom-only content, acknowledge rights, then "
    "discover.\nInstalled stock assets are not automatically "
    "included.\nMissing/excluded/runtime dependencies may make the distribution "
    "incomplete."));
}
void SourceToolsDialog::updateExportEnabled()
{
  m_export->setEnabled(
    m_plan && m_plan->canWrite() && !m_plan->files().empty() && m_rights->isChecked()
    && m_approval->isChecked());
}
void SourceToolsDialog::createPlan()
{
  invalidatePlan();
  if (!m_document || !m_review->isEnabled())
  {
    return;
  }
  try
  {
    auto options = mdl::SourceDependencyPackOptions{};
    options.customRoots = {pathFromQString(m_customRoot->text())};
    options.redistributionRightsAcknowledged = m_rights->isChecked();
    if (!m_bsp->text().trimmed().isEmpty())
    {
      auto error = QString{};
      if (!SourceBuild::validateBsp(m_bsp->text(), m_target, error))
      {
        m_packText->setPlainText("BSP refused: " + error);
        return;
      }
      options.compiledBsp = pathFromQString(m_bsp->text());
      if (m_target == SourceBspTarget::Css20)
      {
        options.compiledBspExpectedVersion = 20;
      }
      else if (m_target == SourceBspTarget::Strata25)
      {
        options.compiledBspExpectedVersion = 25;
      }
    }
    m_plan = mdl::planSourceDependencyPack(*m_document, options);
    auto rows = QStringList{
      tr("INCOMPLETE ASSESSMENT — not certification of a licensed/self-contained map."),
      tr("Exact immutable binary snapshots below; changing inputs invalidates approval."),
      options.compiledBsp ? tr("Selected BSP included read-only if allowed by plan.")
                          : tr("No BSP selected: this is an assets-only package.")};
    for (const auto& file : m_plan->files())
    {
      rows << QString{"INCLUDE %1 (%2 bytes, CRC32 %3)"}
                .arg(text(file.archivePath))
                .arg(file.size)
                .arg(file.crc32, 8, 16, QChar{'0'});
    }
    for (const auto& issue : m_plan->issues())
    {
      rows << issueName(issue.kind) + " " + text(issue.reference) + ": "
                + text(issue.message);
    }
    rows
      << (m_plan->canWrite() ? tr(
                                 "Writing permitted after separate approval. Choose a "
                                 "NEW .zip path; existing files are never replaced.")
                             : tr(
                                 "Writing blocked. Resolve the errors/budget/rights "
                                 "findings; do not treat this as a package."));
    m_packText->setPlainText(rows.join('\n'));
    m_approval->setEnabled(m_plan->canWrite() && !m_plan->files().empty());
    updateExportEnabled();
  }
  catch (const std::exception& exception)
  {
    m_plan.reset();
    m_packText->setPlainText("Planning failed: " + QString::fromUtf8(exception.what()));
    updateExportEnabled();
  }
}
void SourceToolsDialog::exportPlan()
{
  if (!m_plan || !m_export->isEnabled())
  {
    return;
  }
  const auto output = QFileDialog::getSaveFileName(
    this,
    tr("NEW custom-content ZIP"),
    {},
    tr("ZIP distribution (*.zip)"),
    nullptr,
    QFileDialog::DontConfirmOverwrite);
  if (output.isEmpty())
  {
    return;
  }
  const auto result = m_plan->writeZip(pathFromQString(output), m_approval->isChecked());
  m_approval->setChecked(false);
  if (!result.written)
  {
    QMessageBox::warning(this, tr("Package not written"), text(result.error));
    return;
  }
  QMessageBox::information(
    this,
    tr("Distribution ZIP written"),
    tr(
      "Wrote the reviewed snapshots to:\n%1\n\nOriginals were not changed. The ZIP is "
      "not proof of licence rights or a self-contained map. Review "
      "excluded/missing/runtime dependencies and test in the intended game.")
      .arg(output));
}
} // namespace tb::ui
