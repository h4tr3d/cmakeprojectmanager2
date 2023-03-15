// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "configmodel.h"

#include "cmakeprojectmanagertr.h"

#include <utils/algorithm.h>
#include <utils/macroexpander.h>
#include <utils/qtcassert.h>
#include <utils/theme/theme.h>

#include <QFont>
#include <QSortFilterProxyModel>

namespace CMakeProjectManager::Internal {

// DataItem

ConfigModel::DataItem::DataItem(const CMakeConfigItem &cmi)
{
    key = QString::fromUtf8(cmi.key);
    value = QString::fromUtf8(cmi.value);
    description = QString::fromUtf8(cmi.documentation);
    values = cmi.values;
    inCMakeCache = cmi.inCMakeCache;

    isAdvanced = cmi.isAdvanced;
    isInitial = cmi.isInitial;
    isHidden = cmi.type == CMakeConfigItem::INTERNAL || cmi.type == CMakeConfigItem::STATIC;

    setType(cmi.type);
}

void ConfigModel::DataItem::setType(CMakeConfigItem::Type cmt)
{
    switch (cmt) {
    case CMakeConfigItem::FILEPATH:
        type = FILE;
        break;
    case CMakeConfigItem::PATH:
        type = DIRECTORY;
        break;
    case CMakeConfigItem::BOOL:
        type = BOOLEAN;
        break;
    case CMakeConfigItem::STRING:
        type = STRING;
        break;
    default:
        type = UNKNOWN;
        break;
    }
}

QString ConfigModel::DataItem::typeDisplay() const
{
    switch (type) {
    case DataItem::BOOLEAN:
        return "BOOL";
    case DataItem::FILE:
        return "FILEPATH";
    case DataItem::DIRECTORY:
        return "PATH";
    case DataItem::STRING:
        return "STRING";
    case DataItem::UNKNOWN:
        break;
    }
    return "UNINITIALIZED";
}

CMakeConfigItem ConfigModel::DataItem::toCMakeConfigItem() const
{
    CMakeConfigItem cmi;
    cmi.key = key.toUtf8();
    cmi.value = value.toUtf8();
    switch (type) {
    case DataItem::BOOLEAN:
        cmi.type = CMakeConfigItem::BOOL;
        break;
    case DataItem::FILE:
        cmi.type = CMakeConfigItem::FILEPATH;
        break;
    case DataItem::DIRECTORY:
        cmi.type = CMakeConfigItem::PATH;
        break;
    case DataItem::STRING:
        cmi.type = CMakeConfigItem::STRING;
        break;
    case DataItem::UNKNOWN:
        cmi.type = CMakeConfigItem::UNINITIALIZED;
        break;
    }
    cmi.isUnset = isUnset;
    cmi.isAdvanced = isAdvanced;
    cmi.isInitial = isInitial;
    cmi.values = values;
    cmi.documentation = description.toUtf8();

    return cmi;
}

QString ConfigModel::DataItem::expandedValue(Utils::MacroExpander *expander)
{
    return toCMakeConfigItem().expandedValue(expander);
}

// ConfigModel

ConfigModel::ConfigModel(QObject *parent) : Utils::TreeModel<>(parent)
{
    setHeader({Tr::tr("Key"), Tr::tr("Value")});
}

ConfigModel::~ConfigModel() = default;

QVariant ConfigModel::data(const QModelIndex &idx, int role) const
{
    // Hide/show groups according to "isAdvanced" setting:
    auto item = static_cast<const Utils::TreeItem *>(idx.internalPointer());
    if (role == ItemIsAdvancedRole && item->childCount() > 0) {
        const bool hasNormalChildren = item->findAnyChild([](const Utils::TreeItem *ti) {
            if (auto cmti = dynamic_cast<const Internal::ConfigModelTreeItem*>(ti))
                return !cmti->dataItem->isAdvanced;
            return false;
        }) != nullptr;
        return hasNormalChildren ? "0" : "1";
    }
    if (role == ItemIsInitialRole && item->childCount() > 0) {
        const bool hasInitialChildren = item->findAnyChild([](const Utils::TreeItem *ti) {
            if (auto cmti = dynamic_cast<const Internal::ConfigModelTreeItem*>(ti))
                return cmti->dataItem->isInitial;
            return false;
        }) != nullptr;
        return hasInitialChildren ? "1" : "0";
    }
    return Utils::TreeModel<>::data(idx, role);
}

bool ConfigModel::setData(const QModelIndex &idx, const QVariant &data, int role)
{
    Utils::TreeItem *item = itemForIndex(idx);
    bool res = item ? item->setData(idx.column(), data, role) : false;
    if (res) {
        const QModelIndex keyIdx = idx.sibling(idx.row(), 0);
        const QModelIndex valueIdx = idx.sibling(idx.row(), 1);
        emit dataChanged(keyIdx, valueIdx);
    }
    return res;
}

void ConfigModel::appendConfiguration(const QString &key,
                                      const QString &value,
                                      const ConfigModel::DataItem::Type type, bool isInitial,
                                      const QString &description,
                                      const QStringList &values)
{
    DataItem item;
    item.key = key;
    item.type = type;
    item.value = value;
    item.isInitial = isInitial;
    item.description = description;
    item.values = values;

    InternalDataItem internalItem(item);
    internalItem.isUserNew = true;

    if (m_kitConfiguration.contains(key))
        internalItem.kitValue = QString::fromUtf8(
            isInitial ? m_kitConfiguration.value(key).value
                      : m_kitConfiguration.value(key).expandedValue(m_macroExpander).toUtf8());
    m_configuration.append(internalItem);
    setConfiguration(m_configuration);
}

void ConfigModel::setConfiguration(const QList<DataItem> &config)
{
    setConfiguration(Utils::transform(config, [](const DataItem &di) { return InternalDataItem(di); }));
}

void ConfigModel::setConfigurationFromKit(const KitConfiguration &kitConfig)
{
    m_kitConfiguration = kitConfig;
    QHash<QString, InternalDataItem> initialConfig;

    // Update the kit values for initial configuration keys
    for (InternalDataItem &i : m_configuration) {
        if (!i.isInitial)
            continue;
        if (m_kitConfiguration.contains(i.key))
            i.kitValue = QString::fromUtf8(m_kitConfiguration.value(i.key).value);
        initialConfig.insert(i.key, i);
    }

    // Add new initial configuration kit keys
    for (const auto &ki : kitConfig) {
        if (!initialConfig.contains(QString::fromUtf8(ki.key))) {
            InternalDataItem i((DataItem(ki)));
            i.isUserNew = true;
            i.isInitial = true;
            i.newValue = i.value;
            i.kitValue = i.value;
            m_configuration.append(i);
        }
    }

    // Remove kit values when the kit's keys are removed
    for (const auto &i : initialConfig) {
        if (!kitConfig.contains(i.key)) {
            auto existing = std::find(m_configuration.begin(), m_configuration.end(), i);
            if (existing != m_configuration.end())
                existing->kitValue.clear();
        }
    }

    setConfiguration(m_configuration);
}

void ConfigModel::flush()
{
    setConfiguration(QList<InternalDataItem>());
}

void ConfigModel::resetAllChanges(bool initialParameters)
{
    QList<InternalDataItem> notNew
            = Utils::filtered(m_configuration,
                              [](const InternalDataItem &i) { return !i.isUserNew; });

    notNew = Utils::transform(notNew, [](const InternalDataItem &i) {
        InternalDataItem ni(i);
        ni.newValue.clear();
        ni.isUserChanged = false;
        ni.isUnset = false;
        return ni;
    });

    // add the changes from the other list, which shouldn't get reset
    notNew.append(Utils::filtered(m_configuration, [initialParameters](const InternalDataItem &i) {
        return !(initialParameters ? i.isInitial : !i.isInitial) && i.isUserNew;
    }));

    setConfiguration(notNew);
}

bool ConfigModel::hasChanges(bool initialParameters) const
{
    const QList<InternalDataItem> filtered
        = Utils::filtered(m_configuration, [initialParameters](const InternalDataItem &i) {
              return initialParameters ? i.isInitial : !i.isInitial;
          });

    return Utils::contains(filtered, [](const InternalDataItem &i) {
        return i.isUserChanged || i.isUserNew || i.isUnset;
    });
}

bool ConfigModel::canForceTo(const QModelIndex &idx, const ConfigModel::DataItem::Type type) const
{
    if (idx.model() != const_cast<ConfigModel *>(this))
        return false;
    Utils::TreeItem *item = itemForIndex(idx);
    auto cmti = dynamic_cast<Internal::ConfigModelTreeItem *>(item);
    return cmti && (cmti->dataItem->type != type);
}

void ConfigModel::forceTo(const QModelIndex &idx, const ConfigModel::DataItem::Type type)
{
    QTC_ASSERT(canForceTo(idx, type), return);
    Utils::TreeItem *item = itemForIndex(idx);
    auto cmti = dynamic_cast<Internal::ConfigModelTreeItem *>(item);

    cmti->dataItem->type = type;
    const QModelIndex valueIdx = idx.sibling(idx.row(), 1);
    emit dataChanged(valueIdx, valueIdx);
}

void ConfigModel::toggleUnsetFlag(const QModelIndex &idx)
{
    Utils::TreeItem *item = itemForIndex(idx);
    auto cmti = dynamic_cast<Internal::ConfigModelTreeItem *>(item);

    QTC_ASSERT(cmti, return);

    cmti->dataItem->isUnset = !cmti->dataItem->isUnset;
    const QModelIndex valueIdx = idx.sibling(idx.row(), 1);
    const QModelIndex keyIdx = idx.sibling(idx.row(), 0);
    emit dataChanged(keyIdx, valueIdx);
}

void ConfigModel::applyKitValue(const QModelIndex &idx)
{
    applyKitOrInitialValue(idx, KitOrInitial::Kit);
}

void ConfigModel::applyInitialValue(const QModelIndex &idx)
{
    applyKitOrInitialValue(idx, KitOrInitial::Initial);
}

void ConfigModel::applyKitOrInitialValue(const QModelIndex &idx, KitOrInitial ki)
{
    Utils::TreeItem *item = itemForIndex(idx);
    auto cmti = dynamic_cast<Internal::ConfigModelTreeItem *>(item);

    QTC_ASSERT(cmti, return );

    auto dataItem = cmti->dataItem;
    const QString &kitOrInitialValue = ki == KitOrInitial::Kit ? dataItem->kitValue
                                                               : dataItem->initialValue;

    // Allow a different value when the user didn't change anything (don't mark the same value as new)
    // But allow the same value (going back) when the user did a change
    const bool canSetValue = (dataItem->value != kitOrInitialValue && !dataItem->isUserChanged)
                             || dataItem->isUserChanged;

    if (!kitOrInitialValue.isEmpty() && canSetValue) {
        dataItem->newValue = kitOrInitialValue;
        dataItem->isUserChanged = dataItem->value != kitOrInitialValue;

        const QModelIndex valueIdx = idx.sibling(idx.row(), 1);
        const QModelIndex keyIdx = idx.sibling(idx.row(), 0);
        emit dataChanged(keyIdx, valueIdx);
    }
}

ConfigModel::DataItem ConfigModel::dataItemFromIndex(const QModelIndex &idx)
{
    const QAbstractItemModel *m = idx.model();
    QModelIndex mIdx = idx;
    while (auto sfpm = qobject_cast<const QSortFilterProxyModel *>(m)) {
        m = sfpm->sourceModel();
        mIdx = sfpm->mapToSource(mIdx);
    }
    auto model = qobject_cast<const ConfigModel *>(m);
    QTC_ASSERT(model, return DataItem());
    const QModelIndex modelIdx = mIdx;

    Utils::TreeItem *item = model->itemForIndex(modelIdx);
    auto cmti = dynamic_cast<Internal::ConfigModelTreeItem *>(item);

    if (cmti && cmti->dataItem) {
        DataItem di;
        di.key = cmti->dataItem->key;
        di.type = cmti->dataItem->type;
        di.isHidden = cmti->dataItem->isHidden;
        di.isAdvanced = cmti->dataItem->isAdvanced;
        di.isInitial = cmti->dataItem->isInitial;
        di.inCMakeCache = cmti->dataItem->inCMakeCache;
        di.value = cmti->dataItem->currentValue();
        di.description = cmti->dataItem->description;
        di.values = cmti->dataItem->values;
        di.isUnset = cmti->dataItem->isUnset;
        return di;
    }
    return DataItem();
}

QList<ConfigModel::DataItem> ConfigModel::configurationForCMake() const
{
    const QList<InternalDataItem> tmp
            = Utils::filtered(m_configuration, [](const InternalDataItem &i) {
        return i.isUserChanged || i.isUserNew || !i.inCMakeCache || i.isUnset;
    });
    return Utils::transform(tmp, [](const InternalDataItem &item) {
        DataItem newItem(item);
        if (item.isUserChanged)
            newItem.value = item.newValue;
        return newItem;
    });
}

void ConfigModel::setConfiguration(const CMakeConfig &config)
{
    setConfiguration(Utils::transform(config.toList(), [](const CMakeConfigItem &i) {
        return DataItem(i);
    }));
}

void ConfigModel::setBatchEditConfiguration(const CMakeConfig &config)
{
    for (const auto &c: config) {
        DataItem di(c);
        auto existing = std::find(m_configuration.begin(), m_configuration.end(), di);
        if (existing != m_configuration.end()) {
            existing->isUnset = c.isUnset;
            const QString newValue = QString::fromUtf8(c.value);
            // Allow a different value when the user didn't change anything (don't mark the same value as new)
            // But allow the same value (going back) when the user did a change
            const bool canSetValue = (existing->value != newValue && !existing->isUserChanged)
                                     || existing->isUserChanged;
            if (!c.isUnset && canSetValue) {
                existing->isUserChanged = existing->value != newValue;
                existing->setType(c.type);
                existing->newValue = newValue;
            }
        } else if (!c.isUnset) {
            InternalDataItem i(di);
            i.isUserNew = true;
            i.newValue = di.value;
            m_configuration.append(i);
        }
    }

    generateTree();
}

void ConfigModel::setInitialParametersConfiguration(const CMakeConfig &config)
{
    for (const auto &c: config) {
        DataItem di(c);
        InternalDataItem i(di);
        i.inCMakeCache = true;
        i.isInitial = true;
        i.newValue = di.value;
        m_configuration.append(i);
    }
    generateTree();
}

void ConfigModel::setConfiguration(const QList<ConfigModel::InternalDataItem> &config)
{
    auto mergeLists = [](const QList<InternalDataItem> &oldList,
                         const QList<InternalDataItem> &newList) -> QList<InternalDataItem> {
        auto newIt = newList.constBegin();
        auto newEndIt = newList.constEnd();
        auto oldIt = oldList.constBegin();
        auto oldEndIt = oldList.constEnd();

        QList<InternalDataItem> result;
        while (newIt != newEndIt && oldIt != oldEndIt) {
            if (oldIt->isUnset) {
                ++oldIt;
            } else if (newIt->isHidden || newIt->isUnset) {
                ++newIt;
            } else if (newIt->key < oldIt->key) {
                // Add new entry:
                result << *newIt;
                ++newIt;
            } else if (newIt->key > oldIt->key) {
                // Keep old user settings, but skip other entries:
                if (oldIt->isUserChanged || oldIt->isUserNew)
                    result << InternalDataItem(*oldIt);
                ++oldIt;
            } else {
                // merge old/new entry:
                InternalDataItem item(*newIt);
                item.newValue = (newIt->value != oldIt->newValue) ? oldIt->newValue : QString();

                // Do not mark as user changed when we have a reset
                if (oldIt->isUserChanged && !oldIt->newValue.isEmpty() &&
                    !newIt->isUserChanged && newIt->newValue.isEmpty() &&
                    oldIt->value == newIt->value)
                    item.newValue.clear();

                item.isUserChanged = !item.newValue.isEmpty() && (item.newValue != item.value);
                result << item;
                ++newIt;
                ++oldIt;
            }
        }

        // Add remaining new entries:
        for (; newIt != newEndIt; ++newIt) {
            if (newIt->isHidden)
                continue;
            result << InternalDataItem(*newIt);
        }

        return result;
    };

    auto isInitial = [](const InternalDataItem &i) { return i.isInitial; };

    QList<InternalDataItem> initialOld;
    QList<InternalDataItem> currentOld;
    std::tie(initialOld, currentOld) = Utils::partition(m_configuration, isInitial);

    QList<InternalDataItem> initialNew;
    QList<InternalDataItem> currentNew;
    std::tie(initialNew, currentNew) = Utils::partition(config, isInitial);

    m_configuration = mergeLists(initialOld, initialNew);
    m_configuration.append(mergeLists(currentOld, currentNew));

    generateTree();
}

Utils::MacroExpander *ConfigModel::macroExpander() const
{
    return m_macroExpander;
}

void ConfigModel::setMacroExpander(Utils::MacroExpander *newExpander)
{
    m_macroExpander = newExpander;
}

void ConfigModel::generateTree()
{
    QHash<QString, InternalDataItem> initialHash;
    for (const InternalDataItem &di : m_configuration)
        if (di.isInitial)
            initialHash.insert(di.key, di);

    auto root = new Utils::TreeItem;
    for (InternalDataItem &di : m_configuration) {
        auto it = initialHash.find(di.key);
        if (it != initialHash.end())
            di.initialValue = it->expandedValue(macroExpander());

        root->appendChild(new Internal::ConfigModelTreeItem(&di));
    }
    setRootItem(root);
}

ConfigModel::InternalDataItem::InternalDataItem(const ConfigModel::DataItem &item) : DataItem(item)
{ }

QString ConfigModel::InternalDataItem::currentValue() const
{
    if (isUnset)
        return value;
    return isUserChanged ? newValue : value;
}


ConfigModelTreeItem::~ConfigModelTreeItem() = default;

QVariant ConfigModelTreeItem::data(int column, int role) const
{
    QTC_ASSERT(column >= 0 && column < 2, return QVariant());

    QTC_ASSERT(dataItem, return QVariant());

    if (firstChild()) {
        // Node with children: Only ever show name:
        if (column == 0)
            return dataItem->key;
        return QVariant();
    }

    // Leaf node:
    if (role == ConfigModel::ItemIsAdvancedRole) {
        if (dataItem->isInitial)
            return "2";
        return dataItem->isAdvanced ? "1" : "0";
    }
    if (role == ConfigModel::ItemIsInitialRole) {
        return dataItem->isInitial ? "1" : "0";
    }
    if (role == ConfigModel::ItemIsUserNew) {
        return dataItem->isUserNew ? "1" : "0";
    }

    auto fontRole = [this] {
        QFont font;
        font.setBold((dataItem->isUserChanged || dataItem->isUserNew) && !dataItem->isUnset);
        font.setStrikeOut((!dataItem->inCMakeCache && !dataItem->isUserNew) || dataItem->isUnset);
        font.setItalic((dataItem->isInitial && !dataItem->kitValue.isEmpty())
                       || (!dataItem->isInitial && !dataItem->initialValue.isEmpty()));
        return font;
    };

    auto foregroundRole = [this](const QString &value) -> QColor {
        bool mismatch = false;
        if (dataItem->isInitial)
            mismatch = !dataItem->kitValue.isEmpty() && dataItem->kitValue != value;
        else
            mismatch = !dataItem->initialValue.isEmpty() && dataItem->initialValue != value;
        return Utils::creatorTheme()->color(mismatch ? Utils::Theme::TextColorError
                                                     : Utils::Theme::TextColorNormal);
    };

    const QString value = currentValue();
    const auto boolValue = CMakeConfigItem::toBool(value);
    const bool isTrue = boolValue.has_value() && boolValue.value();

    switch (role) {
    case Qt::CheckStateRole:
        if (column == 0)
            return QVariant();
        return (dataItem->type == ConfigModel::DataItem::BOOLEAN)
                   ? QVariant(isTrue ? Qt::Checked : Qt::Unchecked)
                   : QVariant();
    case Qt::DisplayRole:
        if (column == 0)
            return dataItem->key.isEmpty() ? Tr::tr("<UNSET>") : dataItem->key;
        return value;
    case Qt::EditRole:
        if (column == 0)
            return dataItem->key;
        return (dataItem->type == ConfigModel::DataItem::BOOLEAN) ? QVariant(isTrue)
                                                                  : QVariant(value);
    case Qt::FontRole:
        return fontRole();
    case Qt::ForegroundRole:
        return foregroundRole(value);
    case Qt::ToolTipRole:
        return toolTip();
    default:
        return QVariant();
    }
}

bool ConfigModelTreeItem::setData(int column, const QVariant &value, int role)
{
    QTC_ASSERT(column >= 0 && column < 2, return false);
    QTC_ASSERT(dataItem, return false);
    if (dataItem->isUnset)
        return false;

    QString newValue = value.toString();
    if (role == Qt::CheckStateRole) {
        if (column != 1)
            return false;
        newValue = QString::fromLatin1(value.toInt() == 0 ? "OFF" : "ON");
    } else if (role != Qt::EditRole) {
        return false;
    }

    switch (column) {
    case 0:
        if (!dataItem->key.isEmpty() && !dataItem->isUserNew)
            return false;
        dataItem->key = newValue;
        dataItem->isUserNew = true;
        return true;
    case 1:
        if (dataItem->value == newValue) {
            dataItem->newValue.clear();
            dataItem->isUserChanged = false;
        } else {
            dataItem->newValue = newValue;
            dataItem->isUserChanged = true;
        }
        return true;
    default:
        return false;
    }
}

Qt::ItemFlags ConfigModelTreeItem::flags(int column) const
{
    if (column < 0 || column >= 2)
        return Qt::NoItemFlags;

    QTC_ASSERT(dataItem, return Qt::NoItemFlags);

    if (dataItem->isUnset)
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;

    if (column == 1) {
        if (dataItem->type == ConfigModel::DataItem::BOOLEAN)
            return Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable;
        else
            return Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable;
    } else {
        Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (dataItem->isUserNew)
            return flags |= Qt::ItemIsEditable;
        return flags;
    }
}

QString ConfigModelTreeItem::toolTip() const
{
    QTC_ASSERT(dataItem, return QString());
    QStringList tooltip;
    if (!dataItem->description.isEmpty())
        tooltip << dataItem->description;

    const QString pattern = "<dt style=\"font-weight:bold\">%1</dt><dd>%2</dd>";
    if (dataItem->isInitial) {
        if (!dataItem->kitValue.isEmpty())
            tooltip << pattern.arg(Tr::tr("Kit:")).arg(dataItem->kitValue);

        tooltip << pattern.arg(Tr::tr("Initial Configuration:")).arg(dataItem->currentValue());
    } else {
        if (!dataItem->initialValue.isEmpty()) {
            tooltip << pattern.arg(Tr::tr("Initial Configuration:"))
                          .arg(dataItem->initialValue);
        }

        if (dataItem->inCMakeCache) {
            tooltip << pattern.arg(Tr::tr("Current Configuration:")).arg(dataItem->currentValue());
        } else {
            tooltip << pattern.arg(Tr::tr("Not in CMakeCache.txt")).arg(QString());
        }
    }
    tooltip << pattern.arg(Tr::tr("Type:")).arg(dataItem->typeDisplay());

    return "<dl style=\"white-space:pre\">" + tooltip.join(QString()) + "</dl>";
}

QString ConfigModelTreeItem::currentValue() const
{
    QTC_ASSERT(dataItem, return QString());
    return dataItem->isUserChanged ? dataItem->newValue : dataItem->value;
}

} // CMakeProjectManager::Internal
