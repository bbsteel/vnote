#include <QtWidgets>
#include "vdirectorytree.h"
#include "dialog/vnewdirdialog.h"
#include "vconfigmanager.h"
#include "dialog/vdirinfodialog.h"
#include "vnote.h"
#include "vdirectory.h"
#include "utils/vutils.h"
#include "veditarea.h"
#include "vconfigmanager.h"
#include "vmainwindow.h"

extern VMainWindow *g_mainWin;

extern VConfigManager *g_config;
extern VNote *g_vnote;

const QString VDirectoryTree::c_infoShortcutSequence = "F2";
const QString VDirectoryTree::c_copyShortcutSequence = "Ctrl+C";
const QString VDirectoryTree::c_cutShortcutSequence = "Ctrl+X";
const QString VDirectoryTree::c_pasteShortcutSequence = "Ctrl+V";

VDirectoryTree::VDirectoryTree(QWidget *parent)
    : QTreeWidget(parent), VNavigationMode(),
      m_editArea(NULL)
{
    setColumnCount(1);
    setHeaderHidden(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    initShortcuts();
    initActions();

    connect(this, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(handleItemExpanded(QTreeWidgetItem*)));
    connect(this, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
            this, SLOT(handleItemCollapsed(QTreeWidgetItem*)));
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(contextMenuRequested(QPoint)));
    connect(this, &VDirectoryTree::currentItemChanged,
            this, &VDirectoryTree::currentDirectoryItemChanged);
}

void VDirectoryTree::initShortcuts()
{
    QShortcut *infoShortcut = new QShortcut(QKeySequence(c_infoShortcutSequence), this);
    infoShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(infoShortcut, &QShortcut::activated,
            this, [this](){
                editDirectoryInfo();
            });

    QShortcut *copyShortcut = new QShortcut(QKeySequence(c_copyShortcutSequence), this);
    copyShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(copyShortcut, &QShortcut::activated,
            this, [this](){
                copySelectedDirectories();
            });

    QShortcut *cutShortcut = new QShortcut(QKeySequence(c_cutShortcutSequence), this);
    cutShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(cutShortcut, &QShortcut::activated,
            this, [this](){
                cutSelectedDirectories();
            });

    QShortcut *pasteShortcut = new QShortcut(QKeySequence(c_pasteShortcutSequence), this);
    pasteShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(pasteShortcut, &QShortcut::activated,
            this, [this](){
                pasteDirectoriesFromClipboard();
            });
}

void VDirectoryTree::initActions()
{
    newRootDirAct = new QAction(QIcon(":/resources/icons/create_rootdir.svg"),
                                tr("New &Root Folder"), this);
    newRootDirAct->setToolTip(tr("Create a root folder in current notebook"));
    connect(newRootDirAct, &QAction::triggered,
            this, &VDirectoryTree::newRootDirectory);

    newSubDirAct = new QAction(tr("&New Subfolder"), this);
    newSubDirAct->setToolTip(tr("Create a subfolder"));
    connect(newSubDirAct, &QAction::triggered,
            this, &VDirectoryTree::newSubDirectory);

    deleteDirAct = new QAction(QIcon(":/resources/icons/delete_dir.svg"),
                               tr("&Delete"), this);
    deleteDirAct->setToolTip(tr("Delete selected folder"));
    connect(deleteDirAct, &QAction::triggered,
            this, &VDirectoryTree::deleteSelectedDirectory);

    dirInfoAct = new QAction(QIcon(":/resources/icons/dir_info.svg"),
                             tr("&Info\t%1").arg(VUtils::getShortcutText(c_infoShortcutSequence)), this);
    dirInfoAct->setToolTip(tr("View and edit current folder's information"));
    connect(dirInfoAct, &QAction::triggered,
            this, &VDirectoryTree::editDirectoryInfo);

    copyAct = new QAction(QIcon(":/resources/icons/copy.svg"),
                          tr("&Copy\t%1").arg(VUtils::getShortcutText(c_copyShortcutSequence)), this);
    copyAct->setToolTip(tr("Copy selected folders"));
    connect(copyAct, &QAction::triggered,
            this, &VDirectoryTree::copySelectedDirectories);

    cutAct = new QAction(QIcon(":/resources/icons/cut.svg"),
                         tr("C&ut\t%1").arg(VUtils::getShortcutText(c_cutShortcutSequence)), this);
    cutAct->setToolTip(tr("Cut selected folders"));
    connect(cutAct, &QAction::triggered,
            this, &VDirectoryTree::cutSelectedDirectories);

    pasteAct = new QAction(QIcon(":/resources/icons/paste.svg"),
                           tr("&Paste\t%1").arg(VUtils::getShortcutText(c_pasteShortcutSequence)), this);
    pasteAct->setToolTip(tr("Paste folders in this folder"));
    connect(pasteAct, &QAction::triggered,
            this, &VDirectoryTree::pasteDirectoriesFromClipboard);

    m_openLocationAct = new QAction(tr("&Open Folder Location"), this);
    m_openLocationAct->setToolTip(tr("Open the folder containing this folder in operating system"));
    connect(m_openLocationAct, &QAction::triggered,
            this, &VDirectoryTree::openDirectoryLocation);

    m_reloadAct = new QAction(tr("&Reload From Disk"), this);
    m_reloadAct->setToolTip(tr("Reload the content of this folder (or notebook) from disk"));
    connect(m_reloadAct, &QAction::triggered,
            this, &VDirectoryTree::reloadFromDisk);

    m_sortAct = new QAction(QIcon(":/resources/icons/sort.svg"),
                            tr("&Sort"),
                            this);
    m_sortAct->setToolTip(tr("Sort folders in this folder/notebook manually"));
    connect(m_sortAct, SIGNAL(triggered(bool)),
            this, SLOT(sortItems()));
}

void VDirectoryTree::setNotebook(VNotebook *p_notebook)
{
    if (m_notebook == p_notebook) {
        return;
    }

    clear();
    m_notebook = p_notebook;
    if (!m_notebook) {
        return;
    }

    if (!m_notebook->open()) {
        VUtils::showMessage(QMessageBox::Warning,
                            tr("Warning"),
                            tr("Fail to open notebook <span style=\"%1\">%2</span>.")
                              .arg(g_config->c_dataTextStyle)
                              .arg(m_notebook->getName()),
                            tr("Please check if the notebook's root folder <span style=\"%1\">%2</span> exists.")
                              .arg(g_config->c_dataTextStyle)
                              .arg(m_notebook->getPath()),
                            QMessageBox::Ok,
                            QMessageBox::Ok,
                            this);
        return;
    }

    updateDirectoryTree();
}

void VDirectoryTree::fillTreeItem(QTreeWidgetItem *p_item, VDirectory *p_directory)
{
    int col = 0;
    QString name = p_directory->getName();
    p_item->setText(col, name);
    p_item->setToolTip(col, name);
    p_item->setData(col, Qt::UserRole, QVariant::fromValue(p_directory));
    p_item->setIcon(col, QIcon(":/resources/icons/dir_item.svg"));
}

void VDirectoryTree::updateDirectoryTree()
{
    clear();

    VDirectory *rootDir = m_notebook->getRootDir();
    const QVector<VDirectory *> &subDirs = rootDir->getSubDirs();
    for (int i = 0; i < subDirs.size(); ++i) {
        VDirectory *dir = subDirs[i];
        QTreeWidgetItem *item = new QTreeWidgetItem(this);

        fillTreeItem(item, dir);

        buildSubTree(item, 1);
    }

    if (!restoreCurrentItem() && topLevelItemCount() > 0) {
        setCurrentItem(topLevelItem(0));
    }
}

bool VDirectoryTree::restoreCurrentItem()
{
    auto it = m_notebookCurrentDirMap.find(m_notebook);
    if (it != m_notebookCurrentDirMap.end()) {
        bool rootDirectory;
        QTreeWidgetItem *item = findVDirectory(it.value(), rootDirectory);
        if (item) {
            setCurrentItem(item);
            return true;
        }
    }

    return false;
}

void VDirectoryTree::buildSubTree(QTreeWidgetItem *p_parent, int p_depth)
{
    if (p_depth == 0) {
        return;
    }

    Q_ASSERT(p_parent);

    VDirectory *dir = getVDirectory(p_parent);
    if (!dir->open()) {
        VUtils::showMessage(QMessageBox::Warning,
                            tr("Warning"),
                            tr("Fail to open folder <span style=\"%1\">%2</span>.")
                              .arg(g_config->c_dataTextStyle)
                              .arg(dir->getName()),
                            tr("Please check if directory <span style=\"%1\">%2</span> exists.")
                              .arg(g_config->c_dataTextStyle)
                              .arg(dir->fetchPath()),
                            QMessageBox::Ok,
                            QMessageBox::Ok,
                            this);
        return;
    }

    if (p_parent->childCount() > 0) {
        // This directory has been built before. Try its children directly.
        int cnt = p_parent->childCount();
        for (int i = 0; i < cnt; ++i) {
            buildSubTree(p_parent->child(i), p_depth -1);
        }
    } else {
        const QVector<VDirectory *> &subDirs = dir->getSubDirs();
        for (int i = 0; i < subDirs.size(); ++i) {
            VDirectory *subDir = subDirs[i];
            QTreeWidgetItem *item = new QTreeWidgetItem(p_parent);
            fillTreeItem(item, subDir);
            buildSubTree(item, p_depth - 1);
        }
    }

    if (dir->isExpanded()) {
        expandItem(p_parent);
    }
}

void VDirectoryTree::handleItemCollapsed(QTreeWidgetItem *p_item)
{
    if (p_item) {
        VDirectory *dir = getVDirectory(p_item);
        dir->setExpanded(false);
    }
}

void VDirectoryTree::handleItemExpanded(QTreeWidgetItem *p_item)
{
    if (p_item) {
        buildChildren(p_item);

        VDirectory *dir = getVDirectory(p_item);
        dir->setExpanded(true);
    }
}

void VDirectoryTree::buildChildren(QTreeWidgetItem *p_item)
{
    Q_ASSERT(p_item);
    int nrChild = p_item->childCount();
    if (nrChild == 0) {
        return;
    }

    for (int i = 0; i < nrChild; ++i) {
        QTreeWidgetItem *childItem = p_item->child(i);
        if (childItem->childCount() > 0) {
            continue;
        }

        buildSubTree(childItem, 1);
    }
}

void VDirectoryTree::updateItemDirectChildren(QTreeWidgetItem *p_item)
{
    QPointer<VDirectory> parentDir;
    if (p_item) {
        parentDir = getVDirectory(p_item);
    } else {
        parentDir = m_notebook->getRootDir();
    }

    const QVector<VDirectory *> &dirs = parentDir->getSubDirs();

    QHash<VDirectory *, QTreeWidgetItem *> itemDirMap;
    int nrChild = p_item ? p_item->childCount() : topLevelItemCount();
    for (int i = 0; i < nrChild; ++i) {
        QTreeWidgetItem *item = p_item ? p_item->child(i) : topLevelItem(i);
        itemDirMap.insert(getVDirectory(item), item);
    }

    for (int i = 0; i < dirs.size(); ++i) {
        VDirectory *dir = dirs[i];
        QTreeWidgetItem *item = itemDirMap.value(dir, NULL);
        if (item) {
            if (p_item) {
                p_item->removeChild(item);
                p_item->insertChild(i, item);
            } else {
                int topIdx = indexOfTopLevelItem(item);
                takeTopLevelItem(topIdx);
                insertTopLevelItem(i, item);
            }

            itemDirMap.remove(dir);
        } else {
            // Insert a new item
            if (p_item) {
                item = new QTreeWidgetItem(p_item);
            } else {
                item = new QTreeWidgetItem(this);
            }

            fillTreeItem(item, dir);
            buildSubTree(item, 1);
        }

        expandSubTree(item);
    }

    // Delete items without corresponding VDirectory
    for (auto iter = itemDirMap.begin(); iter != itemDirMap.end(); ++iter) {
        QTreeWidgetItem *item = iter.value();
        if (p_item) {
            p_item->removeChild(item);
        } else {
            int topIdx = indexOfTopLevelItem(item);
            takeTopLevelItem(topIdx);
        }

        delete item;
    }
}

void VDirectoryTree::contextMenuRequested(QPoint pos)
{
    QTreeWidgetItem *item = itemAt(pos);

    if (!m_notebook) {
        return;
    }

    QMenu menu(this);
    menu.setToolTipsVisible(true);

    if (!item) {
        // Context menu on the free space of the QTreeWidget
        menu.addAction(newRootDirAct);

        if (topLevelItemCount() > 1) {
            menu.addAction(m_sortAct);
        }
    } else {
        // Context menu on a QTreeWidgetItem
        if (item->parent()) {
            // Low-level item
            menu.addAction(newSubDirAct);
        } else {
            // Top-level item
            menu.addAction(newRootDirAct);
            menu.addAction(newSubDirAct);
        }

        if (item->childCount() > 1) {
            menu.addAction(m_sortAct);
        }

        menu.addSeparator();
        menu.addAction(deleteDirAct);
        menu.addAction(copyAct);
        menu.addAction(cutAct);
    }

    if (pasteAvailable()) {
        if (!item) {
            menu.addSeparator();
        }

        menu.addAction(pasteAct);
    }

    menu.addSeparator();
    menu.addAction(m_reloadAct);

    if (item) {
        menu.addAction(m_openLocationAct);
        menu.addAction(dirInfoAct);
    }

    menu.exec(mapToGlobal(pos));
}

void VDirectoryTree::newSubDirectory()
{
    if (!m_notebook) {
        return;
    }

    QTreeWidgetItem *curItem = currentItem();
    if (!curItem) {
        return;
    }

    VDirectory *curDir = getVDirectory(curItem);

    QString info = tr("Create a subfolder in <span style=\"%1\">%2</span>.")
                     .arg(g_config->c_dataTextStyle)
                     .arg(curDir->getName());
    QString defaultName("new_folder");
    defaultName = VUtils::getFileNameWithSequence(curDir->fetchPath(), defaultName);
    VNewDirDialog dialog(tr("Create Folder"), info, defaultName, curDir, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getNameInput();
        QString msg;
        VDirectory *subDir = curDir->createSubDirectory(name, &msg);
        if (!subDir) {
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to create subfolder <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(name),
                                msg,
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);
            return;
        }

        updateItemDirectChildren(curItem);

        locateDirectory(subDir);
    }
}

void VDirectoryTree::newRootDirectory()
{
    if (!m_notebook) {
        return;
    }

    VDirectory *rootDir = m_notebook->getRootDir();

    QString info = tr("Create a root folder in notebook <span style=\"%1\">%2</span>.")
                     .arg(g_config->c_dataTextStyle)
                     .arg(m_notebook->getName());
    QString defaultName("new_folder");
    defaultName = VUtils::getFileNameWithSequence(rootDir->fetchPath(), defaultName);
    VNewDirDialog dialog(tr("Create Root Folder"), info, defaultName, rootDir, this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getNameInput();
        QString msg;
        VDirectory *dir = rootDir->createSubDirectory(name, &msg);
        if (!dir) {
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to create root folder <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(name),
                                msg,
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);
            return;
        }

        updateItemDirectChildren(NULL);

        locateDirectory(dir);
    }
}

void VDirectoryTree::deleteSelectedDirectory()
{
    Q_ASSERT(selectedItems().size() <= 1);

    QTreeWidgetItem *curItem = currentItem();
    if (!curItem) {
        return;
    }

    VDirectory *curDir = getVDirectory(curItem);
    int ret = VUtils::showMessage(QMessageBox::Warning,
                                  tr("Warning"),
                                  tr("Are you sure to delete folder <span style=\"%1\">%2</span>?")
                                    .arg(g_config->c_dataTextStyle)
                                    .arg(curDir->getName()),
                                  tr("<span style=\"%1\">WARNING</span>: "
                                     "VNote will delete the whole directory "
                                     "<span style=\"%2\">%3</span>."
                                     "You could find deleted files in the recycle bin "
                                     "of this folder.<br>"
                                     "The operation is IRREVERSIBLE!")
                                    .arg(g_config->c_warningTextStyle)
                                    .arg(g_config->c_dataTextStyle)
                                    .arg(curDir->fetchPath()),
                                  QMessageBox::Ok | QMessageBox::Cancel,
                                  QMessageBox::Ok,
                                  this,
                                  MessageBoxType::Danger);

    if (ret == QMessageBox::Ok) {
        int nrDeleted = 1;
        m_editArea->closeFile(curDir, true);

        // Remove the item from the tree.
        delete curItem;

        QString msg;
        QString dirName = curDir->getName();
        QString dirPath = curDir->fetchPath();
        if (!VDirectory::deleteDirectory(curDir, false, &msg)) {
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to delete folder <span style=\"%1\">%2</span>.<br>"
                                   "Please check <span style=\"%1\">%3</span> and manually delete it.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(dirName)
                                  .arg(dirPath),
                                msg,
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);
        } else {
            g_mainWin->showStatusMessage(tr("%1 %2 deleted")
                                           .arg(nrDeleted)
                                           .arg(nrDeleted > 1 ? tr("folders") : tr("folder")));
        }
    }
}

void VDirectoryTree::currentDirectoryItemChanged(QTreeWidgetItem *currentItem)
{
    if (!currentItem) {
        emit currentDirectoryChanged(NULL);
        return;
    }

    QPointer<VDirectory> dir = getVDirectory(currentItem);
    m_notebookCurrentDirMap[m_notebook] = dir;
    emit currentDirectoryChanged(dir);
}

void VDirectoryTree::editDirectoryInfo()
{
    QTreeWidgetItem *curItem = currentItem();
    if (!curItem) {
        return;
    }

    VDirectory *curDir = getVDirectory(curItem);
    QString curName = curDir->getName();

    VDirInfoDialog dialog(tr("Folder Information"),
                          "",
                          curDir,
                          curDir->getParentDirectory(),
                          this);
    if (dialog.exec() == QDialog::Accepted) {
        QString name = dialog.getNameInput();
        if (name == curName) {
            return;
        }

        if (!curDir->rename(name)) {
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to rename folder <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(curName),
                                "",
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);
            return;
        }

        fillTreeItem(curItem, curDir);

        emit directoryUpdated(curDir);
    }
}

void VDirectoryTree::openDirectoryLocation() const
{
    QTreeWidgetItem *curItem = currentItem();
    V_ASSERT(curItem);
    QUrl url = QUrl::fromLocalFile(getVDirectory(curItem)->fetchBasePath());
    QDesktopServices::openUrl(url);
}

void VDirectoryTree::reloadFromDisk()
{
    if (!m_notebook) {
        return;
    }

    QString msg;
    QString info;
    VDirectory *curDir = NULL;
    QTreeWidgetItem *curItem = currentItem();
    if (curItem) {
        // Reload current directory.
        curDir = getVDirectory(curItem);
        info = tr("Are you sure to reload folder <span style=\"%1\">%2</span>?")
                 .arg(g_config->c_dataTextStyle).arg(curDir->getName());
        msg = tr("Folder %1 reloaded from disk").arg(curDir->getName());
    } else {
        // Reload notebook.
        info = tr("Are you sure to reload notebook <span style=\"%1\">%2</span>?")
                 .arg(g_config->c_dataTextStyle).arg(m_notebook->getName());
        msg = tr("Notebook %1 reloaded from disk").arg(m_notebook->getName());
    }

    if (g_config->getConfirmReloadFolder()) {
        int ret = VUtils::showMessage(QMessageBox::Information, tr("Information"),
                                      info,
                                      tr("VNote will close all the related notes before reload."),
                                      QMessageBox::Ok | QMessageBox::YesToAll | QMessageBox::Cancel,
                                      QMessageBox::Ok,
                                      this);
        switch (ret) {
        case QMessageBox::YesToAll:
            g_config->setConfirmReloadFolder(false);
            // Fall through.

        case QMessageBox::Ok:
            break;

        case QMessageBox::Cancel:
            return;

        default:
            return;
        }
    }

    m_notebookCurrentDirMap.remove(m_notebook);

    if (curItem) {
        if (!m_editArea->closeFile(curDir, false)) {
            return;
        }

        setCurrentItem(NULL);

        curItem->setExpanded(false);
        curDir->setExpanded(false);

        curDir->close();

        // Remove all its children.
        QList<QTreeWidgetItem *> children = curItem->takeChildren();
        for (int i = 0; i < children.size(); ++i) {
            delete children[i];
        }

        buildSubTree(curItem, 1);

        setCurrentItem(curItem);
    } else {
        if (!m_editArea->closeFile(m_notebook, false)) {
            return;
        }

        m_notebook->close();

        if (!m_notebook->open()) {
            VUtils::showMessage(QMessageBox::Warning, tr("Warning"),
                                tr("Fail to open notebook <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle).arg(m_notebook->getName()),
                                tr("Please check if path <span style=\"%1\">%2</span> exists.")
                                  .arg(g_config->c_dataTextStyle).arg(m_notebook->getPath()),
                                QMessageBox::Ok, QMessageBox::Ok, this);
            clear();
            return;
        }

        updateDirectoryTree();
    }

    if (!msg.isEmpty()) {
        g_mainWin->showStatusMessage(msg);
    }
}

void VDirectoryTree::copySelectedDirectories(bool p_isCut)
{
    QList<QTreeWidgetItem *> items = selectedItems();
    if (items.isEmpty()) {
        return;
    }

    QJsonArray dirs;
    for (int i = 0; i < items.size(); ++i) {
        VDirectory *dir = getVDirectory(items[i]);
        dirs.append(dir->fetchPath());
    }

    QJsonObject clip;
    clip[ClipboardConfig::c_magic] = getNewMagic();
    clip[ClipboardConfig::c_type] = (int)ClipboardOpType::CopyDir;
    clip[ClipboardConfig::c_isCut] = p_isCut;
    clip[ClipboardConfig::c_dirs] = dirs;

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(QJsonDocument(clip).toJson(QJsonDocument::Compact));

    qDebug() << "copied directories info" << clipboard->text();

    int cnt = dirs.size();
    g_mainWin->showStatusMessage(tr("%1 %2 %3")
                                   .arg(cnt)
                                   .arg(cnt > 1 ? tr("folders") : tr("folder"))
                                   .arg(p_isCut ? tr("cut") : tr("copied")));
}

void VDirectoryTree::cutSelectedDirectories()
{
    copySelectedDirectories(true);
}

void VDirectoryTree::pasteDirectoriesFromClipboard()
{
    if (!pasteAvailable()) {
        return;
    }

    QJsonObject obj = VUtils::clipboardToJson();
    QJsonArray dirs = obj[ClipboardConfig::c_dirs].toArray();
    bool isCut = obj[ClipboardConfig::c_isCut].toBool();
    QVector<QString> dirsToPaste(dirs.size());
    for (int i = 0; i < dirs.size(); ++i) {
        dirsToPaste[i] = dirs[i].toString();
    }

    VDirectory *destDir;
    QTreeWidgetItem *item = currentItem();
    if (item) {
        destDir = getVDirectory(item);
    } else {
        destDir = m_notebook->getRootDir();
    }

    pasteDirectories(destDir, dirsToPaste, isCut);

    QClipboard *clipboard = QApplication::clipboard();
    clipboard->clear();
}

void VDirectoryTree::pasteDirectories(VDirectory *p_destDir,
                                      const QVector<QString> &p_dirs,
                                      bool p_isCut)
{
    if (!p_destDir || p_dirs.isEmpty()) {
        return;
    }

    int nrPasted = 0;
    for (int i = 0; i < p_dirs.size(); ++i) {
        VDirectory *dir = g_vnote->getInternalDirectory(p_dirs[i]);
        if (!dir) {
            qWarning() << "Copied dir is not an internal folder" << p_dirs[i];
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to paste folder <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(p_dirs[i]),
                                tr("VNote could not find this folder in any notebook."),
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);

            continue;
        }

        if (dir == p_destDir) {
            continue;
        }

        QString dirName = dir->getName();
        VDirectory *paDir = dir->getParentDirectory();
        if (paDir == p_destDir) {
            if (p_isCut) {
                continue;
            }

            // Copy and paste in the same folder.
            // Rename it to xxx_copy.
            dirName = VUtils::generateCopiedDirName(paDir->fetchPath(), dirName);
        } else {
            // Rename it to xxx_copy if needed.
            dirName = VUtils::generateCopiedDirName(p_destDir->fetchPath(), dirName);
        }

        QString msg;
        VDirectory *destDir = NULL;
        bool ret = VDirectory::copyDirectory(p_destDir,
                                             dirName,
                                             dir,
                                             p_isCut,
                                             &destDir,
                                             &msg);
        if (!ret) {
            VUtils::showMessage(QMessageBox::Warning,
                                tr("Warning"),
                                tr("Fail to copy folder <span style=\"%1\">%2</span>.")
                                  .arg(g_config->c_dataTextStyle)
                                  .arg(p_dirs[i]),
                                msg,
                                QMessageBox::Ok,
                                QMessageBox::Ok,
                                this);
        }

        if (destDir) {
            ++nrPasted;

            // Update QTreeWidget.
            bool isWidget;
            QTreeWidgetItem *destItem = findVDirectory(p_destDir, isWidget);
            if (destItem || isWidget) {
                updateItemDirectChildren(destItem);
            }

            if (p_isCut) {
                QTreeWidgetItem *srcItem = findVDirectory(paDir, isWidget);
                if (srcItem || isWidget) {
                    updateItemDirectChildren(srcItem);
                }
            }

            // Broadcast this update
            emit directoryUpdated(destDir);
        }
    }

    qDebug() << "pasted" << nrPasted << "directories";
    if (nrPasted > 0) {
        g_mainWin->showStatusMessage(tr("%1 %2 pasted")
                                       .arg(nrPasted)
                                       .arg(nrPasted > 1 ? tr("folders") : tr("folder")));
    }

    getNewMagic();
}

bool VDirectoryTree::pasteAvailable() const
{
    QJsonObject obj = VUtils::clipboardToJson();
    if (obj.isEmpty()) {
        return false;
    }

    if (!obj.contains(ClipboardConfig::c_type)) {
        return false;
    }

    ClipboardOpType type = (ClipboardOpType)obj[ClipboardConfig::c_type].toInt();
    if (type != ClipboardOpType::CopyDir) {
        return false;
    }

    if (!obj.contains(ClipboardConfig::c_magic)
        || !obj.contains(ClipboardConfig::c_isCut)
        || !obj.contains(ClipboardConfig::c_dirs)) {
        return false;
    }

    int magic = obj[ClipboardConfig::c_magic].toInt();
    if (!checkMagic(magic)) {
        return false;
    }

    QJsonArray dirs = obj[ClipboardConfig::c_dirs].toArray();
    return !dirs.isEmpty();
}

void VDirectoryTree::mousePressEvent(QMouseEvent *event)
{
    QTreeWidgetItem *item = itemAt(event->pos());
    if (!item) {
        setCurrentItem(NULL);
    }

    QTreeWidget::mousePressEvent(event);
}

void VDirectoryTree::keyPressEvent(QKeyEvent *event)
{
    int key = event->key();
    int modifiers = event->modifiers();

    switch (key) {
    case Qt::Key_Return:
    {
        QTreeWidgetItem *item = currentItem();
        if (item) {
            item->setExpanded(!item->isExpanded());
        }

        break;
    }

    case Qt::Key_J:
    {
        if (modifiers == Qt::ControlModifier) {
            event->accept();
            QKeyEvent *downEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Down,
                                                 Qt::NoModifier);
            QCoreApplication::postEvent(this, downEvent);
            return;
        }

        break;
    }

    case Qt::Key_K:
    {
        if (modifiers == Qt::ControlModifier) {
            event->accept();
            QKeyEvent *upEvent = new QKeyEvent(QEvent::KeyPress, Qt::Key_Up,
                                               Qt::NoModifier);
            QCoreApplication::postEvent(this, upEvent);
            return;
        }

        break;
    }

    case Qt::Key_Asterisk:
    {
        if (modifiers == Qt::ShiftModifier) {
            // *, by default will expand current item recursively.
            // We build the tree recursively before the expanding.
            QTreeWidgetItem *item = currentItem();
            if (item) {
                buildSubTree(item, -1);
            }
        }

        break;
    }

    default:
        break;
    }

    QTreeWidget::keyPressEvent(event);
}

QTreeWidgetItem *VDirectoryTree::findVDirectory(const VDirectory *p_dir, bool &p_widget)
{
    p_widget = false;
    if (!p_dir) {
        return NULL;
    } else if (p_dir->getNotebookName() != m_notebook->getName()) {
        return NULL;
    } else if (p_dir == m_notebook->getRootDir()) {
        p_widget = true;
        return NULL;
    }

    bool isWidget;
    QTreeWidgetItem *pItem = findVDirectory(p_dir->getParentDirectory(), isWidget);
    if (pItem) {
        // Iterate all its children to find the match.
        int nrChild = pItem->childCount();
        for (int i = 0; i < nrChild; ++i) {
            QTreeWidgetItem *item = pItem->child(i);
            if (getVDirectory(item) == p_dir) {
                return item;
            }
        }
    } else if (isWidget) {
        // Iterate all the top-level items.
        int nrChild = topLevelItemCount();
        for (int i = 0; i < nrChild; ++i) {
            QTreeWidgetItem *item = topLevelItem(i);
            if (getVDirectory(item) == p_dir) {
                return item;
            }
        }
    }

    return NULL;
}

bool VDirectoryTree::locateDirectory(const VDirectory *p_directory)
{
    if (p_directory) {
        if (p_directory->getNotebook() != m_notebook) {
            return false;
        }

        QTreeWidgetItem *item = expandToVDirectory(p_directory);
        if (item) {
            setCurrentItem(item);
        }

        return item;
    }

    return false;
}

QTreeWidgetItem *VDirectoryTree::expandToVDirectory(const VDirectory *p_directory)
{
    if (!p_directory
        || p_directory->getNotebook() != m_notebook
        || p_directory == m_notebook->getRootDir()) {
        return NULL;
    }

    if (p_directory->getParentDirectory() == m_notebook->getRootDir()) {
        // Top-level items.
        int nrChild = topLevelItemCount();
        for (int i = 0; i < nrChild; ++i) {
            QTreeWidgetItem *item = topLevelItem(i);
            if (getVDirectory(item) == p_directory) {
                return item;
            }
        }
    } else {
        QTreeWidgetItem *pItem = expandToVDirectory(p_directory->getParentDirectory());
        if (!pItem) {
            return NULL;
        }

        int nrChild = pItem->childCount();
        if (nrChild == 0) {
            buildSubTree(pItem, 1);
        }

        nrChild = pItem->childCount();
        for (int i = 0; i < nrChild; ++i) {
            QTreeWidgetItem *item = pItem->child(i);
            if (getVDirectory(item) == p_directory) {
                return item;
            }
        }
    }

    return NULL;
}

void VDirectoryTree::expandSubTree(QTreeWidgetItem *p_item)
{
    if (!p_item) {
        return;
    }

    VDirectory *dir = getVDirectory(p_item);
    int nrChild = p_item->childCount();
    for (int i = 0; i < nrChild; ++i) {
        expandSubTree(p_item->child(i));
    }

    if (dir->isExpanded()) {
        Q_ASSERT(nrChild > 0);
        expandItem(p_item);
    }
}

void VDirectoryTree::registerNavigation(QChar p_majorKey)
{
    m_majorKey = p_majorKey;
    V_ASSERT(m_keyMap.empty());
    V_ASSERT(m_naviLabels.empty());
}

void VDirectoryTree::showNavigation()
{
    // Clean up.
    m_keyMap.clear();
    for (auto label : m_naviLabels) {
        delete label;
    }
    m_naviLabels.clear();

    if (!isVisible()) {
        return;
    }

    // Generate labels for visible items.
    auto items = getVisibleItems();
    for (int i = 0; i < 26 && i < items.size(); ++i) {
        QChar key('a' + i);
        m_keyMap[key] = items[i];

        QString str = QString(m_majorKey) + key;
        QLabel *label = new QLabel(str, this);
        label->setStyleSheet(g_vnote->getNavigationLabelStyle(str));
        label->move(visualItemRect(items[i]).topLeft());
        label->show();
        m_naviLabels.append(label);
    }
}

void VDirectoryTree::hideNavigation()
{
    m_keyMap.clear();
    for (auto label : m_naviLabels) {
        delete label;
    }
    m_naviLabels.clear();
}

bool VDirectoryTree::handleKeyNavigation(int p_key, bool &p_succeed)
{
    static bool secondKey = false;
    bool ret = false;
    p_succeed = false;
    QChar keyChar = VUtils::keyToChar(p_key);
    if (secondKey && !keyChar.isNull()) {
        secondKey = false;
        p_succeed = true;
        ret = true;
        auto it = m_keyMap.find(keyChar);
        if (it != m_keyMap.end()) {
            setCurrentItem(it.value());
            setFocus();
        }
    } else if (keyChar == m_majorKey) {
        // Major key pressed.
        // Need second key if m_keyMap is not empty.
        if (m_keyMap.isEmpty()) {
            p_succeed = true;
        } else {
            secondKey = true;
        }
        ret = true;
    }
    return ret;
}

QList<QTreeWidgetItem *> VDirectoryTree::getVisibleItems() const
{
    QList<QTreeWidgetItem *> items;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = topLevelItem(i);
        if (!item->isHidden()) {
            items.append(item);
            if (item->isExpanded()) {
                items.append(getVisibleChildItems(item));
            }
        }
    }

    return items;
}

QList<QTreeWidgetItem *> VDirectoryTree::getVisibleChildItems(const QTreeWidgetItem *p_item) const
{
    QList<QTreeWidgetItem *> items;
    if (p_item && !p_item->isHidden() && p_item->isExpanded()) {
        for (int i = 0; i < p_item->childCount(); ++i) {
            QTreeWidgetItem *child = p_item->child(i);
            if (!child->isHidden()) {
                items.append(child);
                if (child->isExpanded()) {
                    items.append(getVisibleChildItems(child));
                }
            }
        }
    }

    return items;
}

int VDirectoryTree::getNewMagic()
{
    m_magicForClipboard = (int)QDateTime::currentDateTime().toTime_t();
    m_magicForClipboard |= qrand();

    return m_magicForClipboard;
}

bool VDirectoryTree::checkMagic(int p_magic) const
{
    return m_magicForClipboard == p_magic;
}

void VDirectoryTree::sortItems()
{
    if (!m_notebook) {
        return;
    }

    QTreeWidgetItem *item = currentItem();
    if (item) {
        sortItems(getVDirectory(item));
    } else {
        sortItems(m_notebook->getRootDir());
    }
}

void VDirectoryTree::sortItems(VDirectory *p_dir)
{
    if (!p_dir) {
        return;
    }

    qDebug() << "sort items of" << p_dir->getName();
}
