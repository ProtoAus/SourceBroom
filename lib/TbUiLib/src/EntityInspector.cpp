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

#include "ui/EntityInspector.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "mdl/EntityDefinition.h"
#include "mdl/EntityNode.h"
#include "mdl/Map.h"
#include "mdl/MapFormat.h"
#include "mdl/Map_Entities.h"
#include "mdl/PropertyDefinition.h"
#include "mdl/WorldNode.h"
#include "ui/EntityBrowser.h"
#include "ui/EntityDefinitionFileChooser.h"
#include "ui/EntityPropertyEditor.h"
#include "ui/MapDocument.h"
#include "ui/Splitter.h"
#include "ui/SwitchableTitledPanel.h"
#include "ui/WidgetState.h"

namespace tb::ui
{
namespace
{
class SourceOutputsEditor : public QWidget
{
  MapDocument& m_document;
  QTableWidget* m_table;
  QPushButton* m_add;
  QPushButton* m_remove;
  QLabel* m_status;
  NotifierConnection m_connection;
  bool m_refreshing = false;

  const mdl::Entity* selected() const
  {
    const auto entities = m_document.map().selection().allEntities();
    if (entities.size() != 1 || !dynamic_cast<mdl::EntityNode*>(entities.front()))
      return nullptr;
    return &entities.front()->entity();
  }

  void refresh()
  {
    m_refreshing = true;
    const auto native =
      m_document.map().worldNode().mapFormat() == mdl::MapFormat::SourceVMF;
    setVisible(native);
    const auto* entity = native ? selected() : nullptr;
    m_table->setRowCount(0);
    m_add->setEnabled(entity != nullptr);
    m_remove->setEnabled(entity != nullptr);
    m_status->setText(tr(
      "Select one entity. Delimiter: comma or ESC. Malformed native rows are preserved "
      "read-only."));
    if (entity)
    {
      auto hints = QString{};
      if (const auto* definition = entity->definition())
      {
        for (const auto& property : definition->propertyDefinitions)
        {
          if (std::holds_alternative<mdl::PropertyValueTypes::Output>(property.valueType))
            hints += tr("Output: ") + QString::fromStdString(property.key) + "\n";
          if (std::holds_alternative<mdl::PropertyValueTypes::Input>(property.valueType))
            hints += tr("Input: ") + QString::fromStdString(property.key) + "\n";
        }
      }
      m_table->setToolTip(hints);
      for (const auto& row : entity->sourceConnections())
      {
        const auto fields = mdl::parseSourceConnection(row.value);
        const auto index = m_table->rowCount();
        m_table->insertRow(index);
        const auto values = fields
          ? QStringList{QString::fromStdString(row.event), QString::fromStdString(fields->target),
              QString::fromStdString(fields->input), QString::fromStdString(fields->parameter),
              QString::fromStdString(fields->delay), QString::fromStdString(fields->maxFires),
              fields->delimiter == '' ? "ESC" : ","}
          : QStringList{QString::fromStdString(row.event), {}, {}, QString::fromStdString(row.value), {}, {}, {}};
        for (int column = 0; column < values.size(); ++column)
        {
          auto* item = new QTableWidgetItem{values[column]};
          if (!fields)
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
          m_table->setItem(index, column, item);
        }
      }
    }
    m_refreshing = false;
  }

  void apply(std::vector<mdl::SourceEntityConnection> rows)
  {
    if (!mdl::setSourceEntityConnections(m_document.map(), std::move(rows)))
    {
      refresh();
      m_status->setText(tr(
        "Output edit refused: check delimiter, fields, nonnegative finite delay and max "
        "fires >= -1."));
    }
  }

public:
  explicit SourceOutputsEditor(MapDocument& document, QWidget* parent)
    : QWidget{parent}
    , m_document{document}
  {
    auto* layout = new QVBoxLayout{this};
    layout->addWidget(new QLabel{tr("Source Outputs"), this});
    m_table = new QTableWidget{0, 7, this};
    m_table->setHorizontalHeaderLabels(
      {tr("Event"),
       tr("Target"),
       tr("Input"),
       tr("Parameter"),
       tr("Delay"),
       tr("Max fires"),
       tr("Delimiter")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_table);
    auto* buttons = new QHBoxLayout{};
    m_add = new QPushButton{tr("Add output"), this};
    m_remove = new QPushButton{tr("Remove output"), this};
    buttons->addWidget(m_add);
    buttons->addWidget(m_remove);
    layout->addLayout(buttons);
    m_status = new QLabel{this};
    m_status->setWordWrap(true);
    layout->addWidget(m_status);
    connect(m_add, &QPushButton::clicked, this, [this] {
      if (const auto* entity = selected())
      {
        auto rows = entity->sourceConnections();
        rows.push_back({"OnUser1", "!self,Use,,0,-1", std::nullopt});
        apply(std::move(rows));
      }
    });
    connect(m_remove, &QPushButton::clicked, this, [this] {
      const auto index = m_table->currentRow();
      if (const auto* entity = selected(); entity && index >= 0)
      {
        auto rows = entity->sourceConnections();
        rows.erase(rows.begin() + index);
        apply(std::move(rows));
      }
    });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int index, int column) {
      if (m_refreshing)
        return;
      if (const auto* entity = selected())
      {
        auto rows = entity->sourceConnections();
        auto& row = rows.at(static_cast<size_t>(index));
        const auto field = static_cast<mdl::SourceConnectionField>(column);
        const auto original = mdl::sourceConnectionFieldText(row, field);
        // Display decoding must never become an implicit conversion of native bytes.
        if (!original || QString::fromStdString(*original).toStdString() != *original)
        {
          refresh();
          m_status->setText(tr(
            "Output edit refused: this field is not lossless UTF-8. Other fields "
            "can still be edited without changing its original bytes."));
          return;
        }
        const auto text = m_table->item(index, column)->text();
        if (QString::fromStdString(text.toStdString()) != text)
        {
          refresh();
          m_status->setText(
            tr("Output edit refused: entered text is not lossless UTF-8."));
          return;
        }
        const auto edited =
          mdl::editSourceConnectionField(row, field, text.toStdString());
        if (!edited)
        {
          refresh();
          m_status->setText(tr("Output edit refused: invalid field or delimiter."));
          return;
        }
        row = *edited;
        apply(std::move(rows));
      }
    });
    m_connection += document.documentWasLoadedNotifier.connect([this] { refresh(); });
    m_connection += document.documentDidChangeNotifier.connect([this] { refresh(); });
    m_connection +=
      document.selectionDidChangeNotifier.connect([this](const auto&) { refresh(); });
    refresh();
  }
};
} // namespace


EntityInspector::EntityInspector(
  AppController& appController, MapDocument& document, QWidget* parent)
  : TabBookPage{parent}
{
  createGui(appController, document);
}

EntityInspector::~EntityInspector()
{
  saveWidgetState(m_splitter);
}

void EntityInspector::createGui(AppController& appController, MapDocument& document)
{
  m_splitter = new Splitter{Qt::Vertical};
  m_splitter->setObjectName("EntityInspector_Splitter");

  m_splitter->addWidget(createEntityPropertyEditor(document, m_splitter));
  m_splitter->addWidget(new SourceOutputsEditor{document, m_splitter});
  m_splitter->addWidget(createEntityBrowser(appController, document, m_splitter));

  // when the window resizes, keep the attribute editor size constant
  m_splitter->setStretchFactor(0, 0);
  m_splitter->setStretchFactor(1, 0);
  m_splitter->setStretchFactor(2, 1);

  auto* layout = new QVBoxLayout{};
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_splitter, 1);
  setLayout(layout);

  restoreWidgetState(m_splitter);
}

QWidget* EntityInspector::createEntityPropertyEditor(
  MapDocument& document, QWidget* parent)
{
  m_attributeEditor = new EntityPropertyEditor{document, parent};
  return m_attributeEditor;
}

QWidget* EntityInspector::createEntityBrowser(
  AppController& appController, MapDocument& document, QWidget* parent)
{
  auto* panel = new SwitchableTitledPanel{
    tr("Entity Browser"), {{tr("Browser"), tr("Settings")}}, parent};

  m_entityBrowser = new EntityBrowser{appController, document};

  auto* entityBrowserLayout = new QVBoxLayout{};
  entityBrowserLayout->setContentsMargins(0, 0, 0, 0);
  entityBrowserLayout->addWidget(m_entityBrowser, 1);
  panel->getPanel(0)->setLayout(entityBrowserLayout);

  auto* entityDefinitionFileEditor = new EntityDefinitionFileChooser{document};

  auto* entityDefinitionFileEditorLayout = new QVBoxLayout{};
  entityDefinitionFileEditorLayout->setContentsMargins(0, 0, 0, 0);
  entityDefinitionFileEditorLayout->addWidget(entityDefinitionFileEditor, 1);
  panel->getPanel(1)->setLayout(entityDefinitionFileEditorLayout);

  return panel;
}

} // namespace tb::ui
