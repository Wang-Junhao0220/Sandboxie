#include "stdafx.h"
#include "SbieView.h"
#include "..\SandMan.h"
#include "../QSbieAPI/SbieAPI.h"
#include "../QSbieAPI/SbieUtils.h"
#include "../../MiscHelpers/Common/SortFilterProxyModel.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../Windows/OptionsWindow.h"
#include "../Windows/SnapshotsWindow.h"
#include "../../MiscHelpers/Common/CheckableMessageBox.h"
#include "../Windows/RecoveryWindow.h"
#include "../Windows/NewBoxWindow.h"
#include "../Views/FileView.h"

#include "qt_windows.h"
#include "qwindowdefs_win.h"
#include <shellapi.h>

CSbieView::CSbieView(QWidget* parent) : CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	//m_UserConfigChanged = false;

	m_pSbieModel = new CSbieModel();
	m_pSbieModel->SetTree(true);
	m_pSbieModel->SetUseIcons(true);

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
	m_pSortProxy->setSourceModel(m_pSbieModel);
	m_pSortProxy->setDynamicSortFilter(true);

	// SbieTree
	m_pSbieTree = new QTreeViewEx();
	m_pSbieTree->setExpandsOnDoubleClick(false);
	//m_pSbieTree->setItemDelegate(theGUI->GetItemDelegate());

	m_pSbieTree->setModel(m_pSortProxy);

	int iViewMode = theConf->GetInt("Options/ViewMode", 1);
	int iLargeIcons = theConf->GetInt("Options/LargeIcons", 2);
	if (iLargeIcons == 2)
		iLargeIcons = iViewMode == 2 ? 1 : 0;
	if (iLargeIcons) {
		m_pSbieModel->SetLargeIcons();
		m_pSbieTree->setIconSize(QSize(32, 32));
	}
	((CSortFilterProxyModel*)m_pSortProxy)->setView(m_pSbieTree);

	m_pSbieTree->setDragDropMode(QAbstractItemView::InternalMove);

	m_pSbieTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pSbieTree->setSortingEnabled(true);
	//m_pSbieTree->setSortingEnabled(false);
	//m_pSbieTree->header()->setSortIndicatorShown(true);
	//m_pSbieTree->header()->setSectionsClickable(true);
	if(iViewMode != 2)
		connect(m_pSbieTree->header(), SIGNAL(sectionClicked(int)), this, SLOT(OnCustomSortByColumn(int)));

	QStyle* pStyle = QStyleFactory::create("windows");
	m_pSbieTree->setStyle(pStyle);

	m_pSbieTree->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pSbieTree, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(OnMenu(const QPoint &)));
	connect(m_pSbieTree, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnDoubleClicked(const QModelIndex&)));
	connect(m_pSbieTree->selectionModel(), SIGNAL(selectionChanged(QItemSelection, QItemSelection)), SLOT(ProcessSelection(QItemSelection, QItemSelection)));
	connect(m_pSbieTree, SIGNAL(expanded(const QModelIndex &)), this, SLOT(OnExpanded(const QModelIndex &)));
	connect(m_pSbieTree, SIGNAL(collapsed(const QModelIndex &)), this, SLOT(OnCollapsed(const QModelIndex &)));

	//connect(theGUI, SIGNAL(ReloadPanels()), m_pSbieModel, SLOT(Clear()));

	connect(m_pSbieModel, SIGNAL(MoveBox(const QString&, const QString&, int)), this, SLOT(OnMoveItem(const QString&, const QString&, int)));
	connect(m_pSbieModel, SIGNAL(MoveGroup(const QString&, const QString&, int)), this, SLOT(OnMoveItem(const QString&, const QString&, int)));

	//m_pSbieTree->setStyleSheet("QTreeView::item:focus {selection-background-color: yellow;}");
	//m_pSbieTree->setFocusPolicy(Qt::NoFocus);

	m_pMainLayout->addWidget(m_pSbieTree);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));


	connect(m_pSbieModel, SIGNAL(ToolTipCallback(const QVariant&, QString&)), this, SLOT(OnToolTipCallback(const QVariant&, QString&)), Qt::DirectConnection);

	if(iViewMode == 2)
		CreateOldMenu();
	else
		CreateMenu();

	CreatTrayMenu();

	QByteArray Columns = theConf->GetBlob("MainWindow/BoxTree_Columns");
	if (Columns.isEmpty())
	{
		m_pSbieTree->OnResetColumns();
		m_pSbieTree->setColumnWidth(0, 300);
		m_pSbieTree->setColumnWidth(1, 70);
		m_pSbieTree->setColumnWidth(2, 70);
		m_pSbieTree->setColumnWidth(3, 70);
	}
	else
		m_pSbieTree->restoreState(Columns);
	if (theConf->GetBool("MainWindow/BoxTree_UseOrder", false) || iViewMode == 2)
		SetCustomOrder();

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu();

	UpdateMenu();
}

CSbieView::~CSbieView()
{
	theConf->SetBlob("MainWindow/BoxTree_Columns", m_pSbieTree->saveState());
	//theConf->SetValue("MainWindow/BoxTree_UseOrder", m_pSortProxy->sortRole() == Qt::InitialSortOrderRole);
}

void CSbieView::Clear()
{
	m_Groups.clear();
	m_pSbieModel->Clear();
}

void CSbieView::CreateMenu()
{
	m_pNewBox = m_pMenu->addAction(CSandMan::GetIcon("NewBox"), tr("Create New Box"), this, SLOT(OnGroupAction()));
	m_pAddGroupe = m_pMenu->addAction(CSandMan::GetIcon("Group"), tr("Create Box Group"), this, SLOT(OnGroupAction()));
	m_pMenu->addSeparator();
	m_pRenGroupe = m_pMenu->addAction(CSandMan::GetIcon("Rename"), tr("Rename Group"), this, SLOT(OnGroupAction()));
	m_pDelGroupe = m_pMenu->addAction(CSandMan::GetIcon("Remove"), tr("Remove Group"), this, SLOT(OnGroupAction()));
	m_pStopAsync = m_pMenu->addAction(CSandMan::GetIcon("Stop"), tr("Stop Operations"), this, SLOT(OnSandBoxAction()));
	m_iMenuTop = m_pMenu->actions().count();
	//m_pMenu->addSeparator();

	m_pMenuRun = m_pMenu->addMenu(CSandMan::GetIcon("Start"), tr("Run"));
		m_pMenuRunAny = m_pMenuRun->addAction(CSandMan::GetIcon("Run"), tr("Run Program"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunMenu = m_pMenuRun->addAction(CSandMan::GetIcon("StartMenu"), tr("Run from Start Menu"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunBrowser = m_pMenuRun->addAction(CSandMan::GetIcon("Internet"), tr("Default Web Browser"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunMailer = m_pMenuRun->addAction(CSandMan::GetIcon("Email"), tr("Default eMail Client"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunCmd = m_pMenuRun->addAction(CSandMan::GetIcon("Cmd"), tr("Command Prompt"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunTools = m_pMenuRun->addMenu(CSandMan::GetIcon("Maintenance"), tr("Boxed Tools"));
			m_pMenuRunCmdAdmin = m_pMenuRunTools->addAction(CSandMan::GetIcon("Cmd"), tr("Command Prompt (as Admin)"), this, SLOT(OnSandBoxAction()));
#ifndef _WIN64
			if(CSbieAPI::IsWow64())
#endif
				m_pMenuRunCmd32 = m_pMenuRunTools->addAction(CSandMan::GetIcon("Cmd"), tr("Command Prompt (32-bit)"), this, SLOT(OnSandBoxAction()));
			m_pMenuRunExplorer = m_pMenuRunTools->addAction(CSandMan::GetIcon("Explore"), tr("Windows Explorer"), this, SLOT(OnSandBoxAction()));
			m_pMenuRunRegEdit = m_pMenuRunTools->addAction(CSandMan::GetIcon("RegEdit"), tr("Registry Editor"), this, SLOT(OnSandBoxAction()));
			m_pMenuRunAppWiz = m_pMenuRunTools->addAction(CSandMan::GetIcon("Software"), tr("Programs and Features"), this, SLOT(OnSandBoxAction()));
			m_pMenuAutoRun = m_pMenuRunTools->addAction(CSandMan::GetIcon("ReloadIni"), tr("Execute Autorun Entries"), this, SLOT(OnSandBoxAction()));
		m_pMenuRun->addSeparator();
		m_iMenuRun = m_pMenuRun->actions().count();
	m_pMenuEmptyBox = m_pMenu->addAction(CSandMan::GetIcon("EmptyAll"), tr("Terminate All Programs"), this, SLOT(OnSandBoxAction()));
	m_pMenu->addSeparator();
	m_pMenuContent = m_pMenu->addMenu(CSandMan::GetIcon("Compatibility"), tr("Box Content"));
		m_pMenuBrowse = m_pMenuContent->addAction(CSandMan::GetIcon("Tree"), tr("Browse Files"), this, SLOT(OnSandBoxAction()));
		m_pMenuContent->addSeparator();
		m_pMenuRefresh = m_pMenuContent->addAction(CSandMan::GetIcon("Refresh"), tr("Refresh Info"), this, SLOT(OnSandBoxAction()));
		m_pMenuMkLink = m_pMenuContent->addAction(CSandMan::GetIcon("MkLink"), tr("Create Shortcut"), this, SLOT(OnSandBoxAction()));
		m_pMenuContent->addSeparator();
		m_pMenuExplore = m_pMenuContent->addAction(CSandMan::GetIcon("Explore"), tr("Explore Content"), this, SLOT(OnSandBoxAction()));
		m_pMenuRegEdit = m_pMenuContent->addAction(CSandMan::GetIcon("RegEdit"), tr("Open Registry"), this, SLOT(OnSandBoxAction()));
	m_pMenuSnapshots = m_pMenu->addAction(CSandMan::GetIcon("Snapshots"), tr("Snapshots Manager"), this, SLOT(OnSandBoxAction()));
	m_pMenuRecover = m_pMenu->addAction(CSandMan::GetIcon("Recover"), tr("Recover Files"), this, SLOT(OnSandBoxAction()));
	m_pMenuCleanUp = m_pMenu->addAction(CSandMan::GetIcon("Erase"), tr("Delete Content"), this, SLOT(OnSandBoxAction()));
	m_pMenu->addSeparator();
	m_pMenuOptions = m_pMenu->addAction(CSandMan::GetIcon("Options"), tr("Sandbox Options"), this, SLOT(OnSandBoxAction()));

	m_pMenuPresets = m_pMenu->addMenu(CSandMan::GetIcon("Presets"), tr("Sandbox Presets"));
		m_pMenuPresetsAdmin = new QActionGroup(m_pMenuPresets);
		m_pMenuPresetsShowUAC = MakeAction(m_pMenuPresetsAdmin, m_pMenuPresets, tr("Ask for UAC Elevation"), 0);
		m_pMenuPresetsNoAdmin = MakeAction(m_pMenuPresetsAdmin, m_pMenuPresets, tr("Drop Admin Rights"), 1);
		m_pMenuPresetsFakeAdmin = MakeAction(m_pMenuPresetsAdmin, m_pMenuPresets, tr("Emulate Admin Rights"), 1 | 2);
		if (theAPI->IsRunningAsAdmin()) {
			m_pMenuPresetsNoAdmin->setEnabled(false);
			m_pMenuPresetsFakeAdmin->setEnabled(false);
		}
		connect(m_pMenuPresetsAdmin, SIGNAL(triggered(QAction*)), this, SLOT(OnSandBoxAction(QAction*)));

		m_pMenuPresets->addSeparator();
		m_pMenuPresetsINet = m_pMenuPresets->addAction(tr("Block Internet Access"), this, SLOT(OnSandBoxAction()));
		m_pMenuPresetsINet->setCheckable(true);
		m_pMenuPresetsShares = m_pMenuPresets->addAction(tr("Allow Network Shares"), this, SLOT(OnSandBoxAction()));
		m_pMenuPresetsShares->setCheckable(true);

		m_pMenuPresets->addSeparator();
		m_pMenuPresetsRecovery = m_pMenuPresets->addAction(tr("Immediate Recovery"), this, SLOT(OnSandBoxAction()));
		m_pMenuPresetsRecovery->setCheckable(true);
	
	m_pMenuTools = m_pMenu->addMenu(CSandMan::GetIcon("Maintenance"), tr("Sandbox Tools"));
		m_pMenuDuplicate = m_pMenuTools->addAction(CSandMan::GetIcon("Duplicate"), tr("Duplicate Box Config"), this, SLOT(OnSandBoxAction()));

	m_pMenuRename = m_pMenu->addAction(CSandMan::GetIcon("Rename"), tr("Rename Sandbox"), this, SLOT(OnSandBoxAction()));
	m_iMoveTo = m_pMenu->actions().count();
	m_pMenuMoveTo = m_pMenu->addMenu(CSandMan::GetIcon("Group"), tr("Move Box/Group"));
		m_pMenuMoveUp = m_pMenuMoveTo->addAction(CSandMan::GetIcon("Up"), tr("Move Up"), this, SLOT(OnGroupAction()));
		m_pMenuMoveUp->setShortcut(QKeySequence("Alt+Up"));
		m_pMenuMoveUp->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		this->addAction(m_pMenuMoveUp);
		//m_pMenuMoveBy = m_pMenuMoveTo->addAction(tr("Move to Position"), this, SLOT(OnGroupAction())); // does not seam that intuitive for users
		m_pMenuMoveDown = m_pMenuMoveTo->addAction(CSandMan::GetIcon("Down"), tr("Move Down"), this, SLOT(OnGroupAction()));
		m_pMenuMoveDown->setShortcut(QKeySequence("Alt+Down"));
		m_pMenuMoveDown->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		this->addAction(m_pMenuMoveDown);
		m_pMenuMoveTo->addSeparator();
	m_pMenuRemove = m_pMenu->addAction(CSandMan::GetIcon("Remove"), tr("Remove Sandbox"), this, SLOT(OnSandBoxAction()));
	m_iMenuBox = m_pMenu->actions().count();


	// Process Menu
	m_pMenuTerminate = m_pMenu->addAction(CSandMan::GetIcon("Remove"), tr("Terminate"), this, SLOT(OnProcessAction()));
	//m_pMenuTerminate->setShortcut(QKeySequence::Delete);
	//m_pMenuTerminate->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pMenuTerminate);
	m_pMenuLinkTo = m_pMenu->addAction(CSandMan::GetIcon("MkLink"), tr("Create Shortcut"), this, SLOT(OnProcessAction()));
	m_pMenuPreset = m_pMenu->addMenu(CSandMan::GetIcon("Presets"), tr("Preset"));
		m_pMenuPinToRun = m_pMenuPreset->addAction(tr("Pin to Run Menu"), this, SLOT(OnProcessAction()));
		m_pMenuPinToRun->setCheckable(true);
		m_pMenuBlackList = m_pMenuPreset->addAction(tr("Block and Terminate"), this, SLOT(OnProcessAction()));
		//m_pMenuBlackList->setShortcut(QKeySequence("Shift+Del"));
		//m_pMenuBlackList->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		this->addAction(m_pMenuBlackList);
		m_pMenuAllowInternet = m_pMenuPreset->addAction(tr("Allow internet access"), this, SLOT(OnProcessAction()));
		m_pMenuAllowInternet->setCheckable(true);
		m_pMenuMarkForced = m_pMenuPreset->addAction(tr("Force into this sandbox"), this, SLOT(OnProcessAction()));
		m_pMenuMarkForced->setCheckable(true);
		m_pMenuMarkLinger = m_pMenuPreset->addAction(tr("Set Linger Process"), this, SLOT(OnProcessAction()));
		m_pMenuMarkLinger->setCheckable(true);
		m_pMenuMarkLeader = m_pMenuPreset->addAction(tr("Set Leader Process"), this, SLOT(OnProcessAction()));
		m_pMenuMarkLeader->setCheckable(true);
	//m_pMenuSuspend = m_pMenu->addAction(tr("Suspend"), this, SLOT(OnProcessAction()));
	//m_pMenuResume = m_pMenu->addAction(tr("Resume"), this, SLOT(OnProcessAction()));
	m_iMenuProc = m_pMenu->actions().count();


	m_pRemove = new QAction(this);
	m_pRemove->setShortcut(QKeySequence::Delete);
	m_pRemove->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemove);
	connect(m_pRemove, SIGNAL(triggered()), this, SLOT(OnRemoveItem()));
}

void CSbieView::CreateOldMenu()
{
	m_pNewBox = m_pMenu->addAction(CSandMan::GetIcon("NewBox"), tr("Create New Box"), this, SLOT(OnGroupAction()));
	m_pAddGroupe = m_pMenu->addAction(CSandMan::GetIcon("Group"), tr("Create Box Group"), this, SLOT(OnGroupAction()));
	m_pMenu->addSeparator();
	m_pRenGroupe = m_pMenu->addAction(CSandMan::GetIcon("Rename"), tr("Rename Group"), this, SLOT(OnGroupAction()));
	m_pDelGroupe = m_pMenu->addAction(CSandMan::GetIcon("Remove"), tr("Remove Group"), this, SLOT(OnGroupAction()));
	m_pStopAsync = m_pMenu->addAction(CSandMan::GetIcon("Stop"), tr("Stop Operations"), this, SLOT(OnSandBoxAction()));
	m_iMenuTop = m_pMenu->actions().count();
	//m_pMenu->addSeparator();

	m_pMenuRun = m_pMenu->addMenu(CSandMan::GetIcon("Start"), tr("Run Sandboxed"));
		m_pMenuRunBrowser = m_pMenuRun->addAction(CSandMan::GetIcon("Internet"), tr("Run Web Browser"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunMailer = m_pMenuRun->addAction(CSandMan::GetIcon("Email"), tr("Run eMail Reader"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunAny = m_pMenuRun->addAction(CSandMan::GetIcon("Run"), tr("Run Any Program"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunMenu = m_pMenuRun->addAction(CSandMan::GetIcon("StartMenu"), tr("Run From Start Menu"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunExplorer = m_pMenuRun->addAction(CSandMan::GetIcon("Explore"), tr("Run Windows Explorer"), this, SLOT(OnSandBoxAction()));
		m_pMenuRunCmd = NULL;
		m_pMenuRunTools = NULL;
			m_pMenuRunCmdAdmin = NULL;
			m_pMenuRunCmd32 = NULL;
			m_pMenuRunRegEdit = NULL;
			m_pMenuRunAppWiz = NULL;
			m_pMenuAutoRun = NULL;
		m_pMenuRun->addSeparator();
		m_iMenuRun = m_pMenuRun->actions().count();

	m_pMenu->addSeparator();
	m_pMenuEmptyBox = m_pMenu->addAction(CSandMan::GetIcon("EmptyAll"), tr("Terminate Programs"), this, SLOT(OnSandBoxAction()));
	m_pMenuRecover = m_pMenu->addAction(CSandMan::GetIcon("Recover"), tr("Quick Recover"), this, SLOT(OnSandBoxAction()));
	m_pMenuCleanUp = m_pMenu->addAction(CSandMan::GetIcon("Erase"), tr("Delete Content"), this, SLOT(OnSandBoxAction()));
	m_pMenuExplore = m_pMenu->addAction(CSandMan::GetIcon("Explore"), tr("Explore Content"), this, SLOT(OnSandBoxAction()));

	m_pMenu->addSeparator();
	m_pMenuOptions = m_pMenu->addAction(CSandMan::GetIcon("Options"), tr("Sandbox Settings"), this, SLOT(OnSandBoxAction()));

	m_pMenuTools = m_pMenu->addMenu(CSandMan::GetIcon("Maintenance"), tr("Sandbox Tools"));
		m_pMenuBrowse = m_pMenuTools->addAction(CSandMan::GetIcon("Tree"), tr("Browse Content"), this, SLOT(OnSandBoxAction()));
		m_pMenuSnapshots = m_pMenuTools->addAction(CSandMan::GetIcon("Snapshots"), tr("Snapshots Manager"), this, SLOT(OnSandBoxAction()));

		m_pMenuTools->addSeparator();
		m_pMenuDuplicate = m_pMenuTools->addAction(CSandMan::GetIcon("Duplicate"), tr("Duplicate Box Config"), this, SLOT(OnSandBoxAction()));

		m_pMenuTools->addSeparator();
		m_pMenuRefresh = m_pMenuTools->addAction(CSandMan::GetIcon("Refresh"), tr("Refresh Info"), this, SLOT(OnSandBoxAction()));
		m_pMenuMkLink = m_pMenuTools->addAction(CSandMan::GetIcon("MkLink"), tr("Create Shortcut"), this, SLOT(OnSandBoxAction()));

	m_pMenu->addSeparator();
	m_pMenuRename = m_pMenu->addAction(CSandMan::GetIcon("Rename"), tr("Rename Sandbox"), this, SLOT(OnSandBoxAction()));
	m_iMoveTo = m_pMenu->actions().count();
	m_pMenuMoveTo = m_pMenu->addMenu(CSandMan::GetIcon("Group"), tr("Move Box/Group"));
		m_pMenuMoveUp = m_pMenuMoveTo->addAction(CSandMan::GetIcon("Up"), tr("Move Up"), this, SLOT(OnGroupAction()));
		m_pMenuMoveUp->setShortcut(QKeySequence("Alt+Up"));
		m_pMenuMoveUp->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		this->addAction(m_pMenuMoveUp);
		//m_pMenuMoveBy = m_pMenuMoveTo->addAction(tr("Move to Position"), this, SLOT(OnGroupAction())); // does not seam that intuitive for users
		m_pMenuMoveDown = m_pMenuMoveTo->addAction(CSandMan::GetIcon("Down"), tr("Move Down"), this, SLOT(OnGroupAction()));
		m_pMenuMoveDown->setShortcut(QKeySequence("Alt+Down"));
		m_pMenuMoveDown->setShortcutContext(Qt::WidgetWithChildrenShortcut);
		this->addAction(m_pMenuMoveDown);
		m_pMenuMoveTo->addSeparator();
	m_pMenuRemove = m_pMenu->addAction(CSandMan::GetIcon("Remove"), tr("Remove Sandbox"), this, SLOT(OnSandBoxAction()));
	m_iMenuBox = m_pMenu->actions().count();


	
	m_pMenuContent = NULL;
		m_pMenuRegEdit = NULL;
	
	m_pMenuPresets = NULL;
		m_pMenuPresetsAdmin = NULL;
		m_pMenuPresetsShowUAC = NULL;
		m_pMenuPresetsNoAdmin = NULL;
		m_pMenuPresetsFakeAdmin = NULL;
		
		m_pMenuPresetsINet = NULL;
		m_pMenuPresetsShares = NULL;

		m_pMenuPresetsRecovery = NULL;
	
	
	// Process Menu
	m_pMenuTerminate = m_pMenu->addAction(CSandMan::GetIcon("Remove"), tr("Terminate"), this, SLOT(OnProcessAction()));
	//m_pMenuTerminate->setShortcut(QKeySequence::Delete);
	//m_pMenuTerminate->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pMenuTerminate);
	m_pMenuLinkTo = m_pMenu->addAction(CSandMan::GetIcon("MkLink"), tr("Create Shortcut"), this, SLOT(OnProcessAction()));
	m_pMenuPreset = NULL;
		m_pMenuPinToRun = NULL;
		m_pMenuBlackList = NULL;
		m_pMenuAllowInternet = NULL;
		m_pMenuMarkForced = NULL;
		m_pMenuMarkLinger = NULL;
		m_pMenuMarkLeader = NULL;
	m_iMenuProc = m_pMenu->actions().count();


	m_pRemove = new QAction(this);
	m_pRemove->setShortcut(QKeySequence::Delete);
	m_pRemove->setShortcutContext(Qt::WidgetWithChildrenShortcut);
	this->addAction(m_pRemove);
	connect(m_pRemove, SIGNAL(triggered()), this, SLOT(OnRemoveItem()));
}

void CSbieView::CreatTrayMenu()
{
	m_pMenu2 = new QMenu();
	m_pMenu2->addMenu(m_pMenuRun);
	m_pMenu2->addAction(m_pMenuEmptyBox);
	m_pMenu2->addSeparator();
	m_pMenu2->addAction(m_pMenuBrowse);
	m_pMenu2->addAction(m_pMenuExplore);
	m_pMenu2->addAction(m_pMenuRegEdit);
	m_pMenu2->addAction(m_pMenuSnapshots);
	m_pMenu2->addAction(m_pMenuRecover);
	m_pMenu2->addAction(m_pMenuCleanUp);
	m_pMenu2->addSeparator();
	if (m_pMenuPresets) {
		m_pMenu2->addAction(m_pMenuOptions);
		m_pMenu2->addMenu(m_pMenuPresets);
	}
}

int CSbieView__ParseGroup(const QString& Grouping, QMap<QString, QStringList>& m_Groups, const QString& Parent = "", int Index = 0)
{
	QRegExp RegExp("[,()]", Qt::CaseInsensitive, QRegExp::RegExp);
	for (; ; )
	{
		int pos = Grouping.indexOf(RegExp, Index);
		QString Name;
		if (pos == -1) {
			Name = Grouping.mid(Index);
			Index = Grouping.length();
		}
		else {
			Name = Grouping.mid(Index, pos - Index);
			Index = pos + 1;
		}
		if (!Name.isEmpty())
			m_Groups[Parent].append(Name);
		if (pos == -1)
			break;
		if (Grouping.at(pos) == "(")
		{
			m_Groups[Name] = QStringList();
			Index = CSbieView__ParseGroup(Grouping, m_Groups, Name, Index);
		}
		else if (Grouping.at(pos) == ")")
			break;
	}
	return Index;
}

QString CSbieView__SerializeGroup(QMap<QString, QStringList>& m_Groups, const QString& Parent = "", QSet<QString> Test = QSet<QString>())
{
	QStringList Grouping;
	foreach(const QString& Name, m_Groups[Parent])
	{
		if (Test.contains(Name))
			continue; // recursion, skil
		Test.insert(Name);
		if (m_Groups.contains(Name))
			Grouping.append(Name + "(" + CSbieView__SerializeGroup(m_Groups, Name, Test) + ")");
		else
			Grouping.append(Name);
	}
	return Grouping.join(",");
}

void CSbieView::Refresh()
{
	QList<QVariant> Added = m_pSbieModel->Sync(theAPI->GetAllBoxes(), m_Groups, theGUI->IsShowHidden());

	if (m_pSbieModel->IsTree())
	{
		QTimer::singleShot(100, this, [this, Added]() {
			foreach(const QVariant ID, Added) {

				QModelIndex ModelIndex = m_pSbieModel->FindIndex(ID);

				if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eProcess)
					m_pSbieTree->expand(m_pSortProxy->mapFromSource(ModelIndex));
				else 
				{
					QString Name;
					if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eGroup)
						Name = m_pSbieModel->GetID(ModelIndex).toString();
					else if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eBox)
						Name = m_pSbieModel->GetSandBox(ModelIndex)->GetName();

					if (!m_Collapsed.contains(Name))
						m_pSbieTree->expand(m_pSortProxy->mapFromSource(ModelIndex));
				}
			}
		});
	}

	// add new boxes to the default group

	foreach(const QStringList &list, m_Groups) {
		foreach(const QString &str, list)
			Added.removeAll(str);
	}

	if (!Added.isEmpty()) {
		foreach(const QVariant& ID, Added) {
			if (ID.type() == QVariant::String) {
				QString id = ID.toString();
				if (id.left(1) != "!")
					m_Groups[""].append(id);
			}
		}

		QString Grouping = CSbieView__SerializeGroup(m_Groups);
		theConf->SetValue("UIConfig/BoxDisplayOrder", Grouping);
	}
}

void CSbieView::OnToolTipCallback(const QVariant& ID, QString& ToolTip)
{
	if (ID.type() == QVariant::String)
	{
		QString BoxName = ID.toString();
		CSandBoxPtr pBox = theAPI->GetBoxByName(BoxName);
		CSandBoxPlus* pBoxEx = qobject_cast<CSandBoxPlus*>(pBox.data());
		if (!pBoxEx)
			return;

		// todo more info

		ToolTip = BoxName + "\n";
		ToolTip += tr("    File root: %1\n").arg(pBoxEx->GetFileRoot());
		ToolTip += tr("    Registry root: %1\n").arg(pBoxEx->GetRegRoot());
		ToolTip += tr("    IPC root: %1\n").arg(pBoxEx->GetIpcRoot());
		
		ToolTip += tr("Options:\n    ");
		ToolTip += pBoxEx->GetStatusStr().replace(", ", "\n    ");
	}
	else if (quint32 ProcessId = ID.toUInt())
	{
		// todo proc info
	}
}

void CSbieView::OnCustomSortByColumn(int column)
{
	Qt::SortOrder order = m_pSbieTree->header()->sortIndicatorOrder();
	//m_pSbieTree->sortByColumn(column, order);
	//m_pSbieTree->header()->setSortIndicatorShown(true);
	if (column == 0) {
		if (m_pSortProxy->sortRole() == Qt::InitialSortOrderRole) {
			m_pSortProxy->sort(0, Qt::AscendingOrder);
			m_pSortProxy->setSortRole(Qt::EditRole);
			theConf->SetValue("MainWindow/BoxTree_UseOrder", false);
			m_pSbieTree->header()->setSortIndicatorShown(true);
		} else if (order == Qt::DescendingOrder) {
			SetCustomOrder();
			theConf->SetValue("MainWindow/BoxTree_UseOrder", true);
		}
	}
	else {
		m_pSortProxy->setSortRole(Qt::EditRole);
		m_pSbieTree->header()->setSortIndicatorShown(true);
	}
}

bool CSbieView::UpdateMenu(bool bAdvanced, const CSandBoxPtr &pBox, int iSandBoxeCount, bool bBoxBusy, const CBoxedProcessPtr &pProcess, int iProcessCount, int iGroupe)
{
	QList<QAction*> MenuActions = m_pMenu->actions();

	//bool isConnected = theAPI->IsConnected();
	//if (isConnected) {
	//	foreach(QAction * pAction, MenuActions) 
	//		pAction->setEnabled(true);
	//}
	

	for (int i = 0; i < m_iMenuTop; i++)
		MenuActions[i]->setVisible(!bBoxBusy && iSandBoxeCount == 0 && iProcessCount == 0);
	m_pStopAsync->setVisible(bBoxBusy);
	m_pRenGroupe->setVisible(iGroupe == 1 && iSandBoxeCount == 0 && iProcessCount == 0);
	m_pDelGroupe->setVisible(iGroupe > 0 && iSandBoxeCount == 0 && iProcessCount == 0);

	for (int i = m_iMenuTop; i < m_iMenuBox; i++)
		MenuActions[i]->setVisible(iSandBoxeCount != 0 && iProcessCount == 0);
	m_pMenuRun->setEnabled(iSandBoxeCount == 1);

	MenuActions[m_iMoveTo]->setVisible((iGroupe > 0 || iSandBoxeCount > 0) && iProcessCount == 0);

	if(iSandBoxeCount == 1)
		UpdateRunMenu(pBox);

	m_pMenuMkLink->setEnabled(iSandBoxeCount == 1);
	m_pMenuTools->setEnabled(iSandBoxeCount == 1);
	m_pMenuRename->setEnabled(iSandBoxeCount == 1);
	m_pMenuRecover->setEnabled(iSandBoxeCount == 1);

	if (m_pMenuPresets) {
		m_pMenuPresets->setEnabled(iSandBoxeCount == 1);
		m_pMenuPresetsShowUAC->setChecked(pBox && !pBox->GetBool("DropAdminRights", false) && !pBox->GetBool("FakeAdminRights", false));
		m_pMenuPresetsNoAdmin->setChecked(pBox && pBox->GetBool("DropAdminRights", false) && !pBox->GetBool("FakeAdminRights", false));
		m_pMenuPresetsFakeAdmin->setChecked(pBox && pBox->GetBool("DropAdminRights", false) && pBox->GetBool("FakeAdminRights", false));
		m_pMenuPresetsINet->setChecked(pBox && pBox.objectCast<CSandBoxPlus>()->IsINetBlocked());
		m_pMenuPresetsShares->setChecked(pBox && pBox.objectCast<CSandBoxPlus>()->HasSharesAccess());
		m_pMenuPresetsRecovery->setChecked(pBox && pBox->GetBool("AutoRecover", false));
	}

	m_pMenuBrowse->setEnabled(iSandBoxeCount == 1);
	m_pMenuExplore->setEnabled(iSandBoxeCount == 1);
	if(m_pMenuRegEdit)m_pMenuRegEdit->setEnabled(iSandBoxeCount == 1);
	m_pMenuOptions->setEnabled(iSandBoxeCount == 1);
	m_pMenuSnapshots->setEnabled(iSandBoxeCount == 1);

	//m_pMenuMoveUp->setEnabled(m_pSortProxy->sortRole() == Qt::InitialSortOrderRole);
	//m_pMenuMoveDown->setEnabled(m_pSortProxy->sortRole() == Qt::InitialSortOrderRole);
	//m_pMenuMoveBy->setEnabled(m_pSortProxy->sortRole() == Qt::InitialSortOrderRole);

	for (int i = m_iMenuBox; i < m_iMenuProc; i++)
		MenuActions[i]->setVisible(iProcessCount != 0 && iSandBoxeCount == 0);
	
	m_pMenuLinkTo->setEnabled(iProcessCount == 1);

	if (!pProcess.isNull()) {
		CSandBoxPlus* pBoxPlus = pProcess.objectCast<CSbieProcess>()->GetBox();
		QStringList RunOptions = pBoxPlus->GetTextList("RunCommand", true);

		QString FoundPin;
		QString FileName = pProcess->GetFileName();
		foreach(const QString& RunOption, RunOptions) {
			QString Cmd = Split2(RunOption, "|").second;
			int pos = Cmd.indexOf(FileName);
			if (pos == 0 || pos == 1) { // 1 for "
				FoundPin = RunOption;
				break;
			}
		}
		if (FoundPin.isEmpty() && FileName.indexOf(pBoxPlus->GetFileRoot(), Qt::CaseInsensitive) == 0) {
			FileName.remove(0, pBoxPlus->GetFileRoot().length());
			foreach(const QString& RunOption, RunOptions) {
				if (Split2(RunOption, "|").second.indexOf(FileName) == 0) {
					FoundPin = RunOption;
					break;
				}
			}
		}

		if (m_pMenuPreset) {
			m_pMenuPinToRun->setChecked(!FoundPin.isEmpty());
			m_pMenuPinToRun->setData(FoundPin);

			m_pMenuAllowInternet->setChecked(pProcess.objectCast<CSbieProcess>()->HasInternetAccess());

			m_pMenuMarkForced->setChecked(pProcess.objectCast<CSbieProcess>()->IsForcedProgram());

			int isLingering = pProcess.objectCast<CSbieProcess>()->IsLingeringProgram();
			m_pMenuMarkLinger->setChecked(isLingering != 0);
			m_pMenuMarkLinger->setEnabled(isLingering != 2);
			m_pMenuMarkLeader->setChecked(pProcess.objectCast<CSbieProcess>()->IsLeaderProgram());
		}
	}

	//m_pMenuSuspend->setEnabled(iProcessCount > iSuspendedCount);
	//m_pMenuResume->setEnabled(iSuspendedCount > 0);

	//if (!isConnected) {
	//	foreach(QAction * pAction, MenuActions)
	//		pAction->setEnabled(false);
	//}

	m_pCopyCell->setVisible(bAdvanced);
	m_pCopyRow->setVisible(bAdvanced);
	m_pCopyPanel->setVisible(bAdvanced);

	return bBoxBusy == false;
}

bool CSbieView::UpdateMenu()
{
	CSandBoxPtr pBox;
	bool bBoxBusy = false;
	CBoxedProcessPtr pProcess;
	int iProcessCount = 0;
	int iSandBoxeCount = 0;
	int iGroupe = 0;
	//int iSuspendedCount = 0;
	QModelIndexList Rows = m_pSbieTree->selectedRows();
	foreach(const QModelIndex& Index, Rows)
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		
		pProcess = m_pSbieModel->GetProcess(ModelIndex);
		if (pProcess)
		{
			iProcessCount++;
			//if (pProcess->IsSuspended())
			//	iSuspendedCount++;
		}
		else
		{
			pBox = m_pSbieModel->GetSandBox(ModelIndex);
			if (pBox)
			{
				if (!pBox->IsEnabled())
					iSandBoxeCount = -1;
				else if (iSandBoxeCount != -1)
					iSandBoxeCount++;

				auto pBoxEx = pBox.objectCast<CSandBoxPlus>();
				if(pBoxEx->IsBusy())
					bBoxBusy = true;
			}
			else
				iGroupe++;
		}
	}

	if (bBoxBusy) {
		iSandBoxeCount = 0;
		iGroupe = 0;
	}

	bool bAdvanced = theConf->GetInt("Options/ViewMode", 1) == 1
		|| (QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier) != 0;

	return UpdateMenu(bAdvanced, pBox, iSandBoxeCount, bBoxBusy, pProcess, iProcessCount, iGroupe);
}

void CSbieView::OnMenu(const QPoint& Point)
{
	if (!theAPI->IsConnected())
		return;

	UpdateMenu();

	CPanelView::OnMenu(Point);
}

void CSbieView::UpdateGroupMenu()
{
	// update move to menu

	foreach(QAction * pAction, m_pMenuMoveTo->actions()) {
		if (!pAction->data().toString().isNull())
			m_pMenuMoveTo->removeAction(pAction);
	}

	foreach(QString Group, m_Groups.keys())
	{
		QString Name = Group;
		
		for (;;) {
			QString Parent = FindParent(Group);
			if (Parent.isEmpty())
				break;
			Group = Parent;
			Name.prepend(Parent + " > ");
		}

		QAction* pAction = m_pMenuMoveTo->addAction(Name.isEmpty() ? tr("[None]") : Name, this, SLOT(OnGroupAction()));
		pAction->setData(Group);
	}
	//m_pMenuMoveTo->setEnabled(m_Groups.keys().count() > 1);
}

void CSbieView::RenameGroup(const QString OldName, const QString NewName)
{
	auto Group = m_Groups.take(OldName);
	m_Groups.insert(NewName, Group);

	RenameItem(OldName, NewName);
}

void CSbieView::RenameItem(const QString OldName, const QString NewName)
{
	for (auto I = m_Groups.begin(); I != m_Groups.end(); ++I)
	{
		if (I.value().removeOne(OldName))
			I.value().append(NewName);
	}
}

QString CSbieView::FindParent(const QString& Name)
{
	for (auto I = m_Groups.begin(); I != m_Groups.end(); ++I)
	{
		if (I.value().contains(Name, Qt::CaseInsensitive))
			return I.key();
	}
	return QString();
}

bool CSbieView::IsParentOf(const QString& Name, const QString& Group)
{
	QString Parent = FindParent(Group);
	if (Parent == Name)
		return true;
	if (Parent.isEmpty())
		return false;
	return IsParentOf(Name, Parent);
}

QStringList CSbieView::GetSelectedGroups(bool bAndBoxes)
{
	QStringList list;
	foreach(const QModelIndex & Index, m_pSbieTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		QString Name;
		if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eGroup)
			Name = m_pSbieModel->GetID(ModelIndex).toString();
		else if (bAndBoxes && m_pSbieModel->GetType(ModelIndex) == CSbieModel::eBox)
			Name = m_pSbieModel->GetSandBox(ModelIndex)->GetName();

		if (Name.isEmpty())
				continue;

		list.append(Name);
	}
	return list;
}

void CSbieView::OnGroupAction()
{
	OnGroupAction(qobject_cast<QAction*>(sender()));
}

void CSbieView::OnGroupAction(QAction* Action)
{
	if (Action == m_pNewBox || Action == m_pAddGroupe)
	{
		QStringList List = GetSelectedGroups();

		QString Name = Action == m_pNewBox ? AddNewBox() : AddNewGroup();
		if (Name.isEmpty())
			return;

		if (List.isEmpty())
			return;

		m_Groups[List.first()].append(Name);
	}
	else if (Action == m_pRenGroupe)
	{
		QStringList List = GetSelectedGroups();
		if (List.isEmpty())
			return;
		
		QString OldValue = List.first();

		QString Value = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter a new name for the Group."), QLineEdit::Normal, OldValue);
		if (Value.isEmpty() || Value == OldValue)
			return;
		if (!TestNameAndWarn(Value))
			return;

		RenameGroup(OldValue, Value);
	}
	else if (Action == m_pDelGroupe)
	{
		if (QMessageBox("Sandboxie-Plus", tr("Do you really want to remove the selected group(s)?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton, this).exec() != QMessageBox::Yes)
			return;

		foreach(const QModelIndex& Index, m_pSbieTree->selectedRows())
		{
			QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
			if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eGroup)
			{
				QString Group = m_pSbieModel->GetID(ModelIndex).toString();

				QStringList Items = m_Groups.take(Group); // remove groupe
		
				// remove from parents
				for (auto I = m_Groups.begin(); I != m_Groups.end(); ++I) {
					if (I.value().removeOne(Group)) {
						// move items to grand parent
						I.value().append(Items);
						break;
					}
				}
			}
		}
	}
	else if (Action == m_pMenuMoveUp /*|| Action == m_pMenuMoveBy*/ || Action == m_pMenuMoveDown)
	{
		if (!theConf->GetBool("MainWindow/BoxTree_UseOrder", false)) {
			SetCustomOrder();
			theConf->SetValue("MainWindow/BoxTree_UseOrder", true);
		}

		int Offset = 0;
		if (Action == m_pMenuMoveUp)
			Offset = -1;
		else if (Action == m_pMenuMoveDown)
			Offset = 1;
		else
			Offset = QInputDialog::getInt(this, "Sandboxie-Plus", tr("Move entries by (negative values move up, positive values move down):"), 0);
		if (Offset == 0)
			return;

		// todo: fix behavioure on multiple selelction
		foreach(const QString& Name, GetSelectedGroups(true)) {
			bool bFound = false;
			retry:
			for (auto I = m_Groups.begin(); I != m_Groups.end(); ++I) {
				int pos = I->indexOf(Name);
				if (pos != -1) {
					if ((Offset < 0 && pos > Offset + 1) ||(Offset > 0 && pos < I->count() - Offset)){
						QString Temp = I.value()[pos+Offset];
						I.value()[pos+Offset] = I.value()[pos];
						I.value()[pos] = Temp;
					}
					bFound = true;
					break;
				}
			}
			if (!bFound) {
				bFound = true;
				m_Groups[""].prepend(Name);
				goto retry;
			}
		}
	}
	else // move to groupe
	{
		QString Group = Action->data().toString();
		
		foreach(const QString& Name, GetSelectedGroups(true))
		{
			if (Name == Group || IsParentOf(Name, Group)) {
				QMessageBox("Sandboxie-Plus", tr("A group can not be its own parent."), QMessageBox::Critical, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
				continue;
			}

			MoveItem(Name, Group);
		}
	}

	if (!(Action == m_pMenuMoveUp /*|| Action == m_pMenuMoveBy*/ || Action == m_pMenuMoveDown)) {
		m_pSbieModel->Clear(); //todo improve that
		Refresh();
	}

	//m_UserConfigChanged = true;
	UpdateGroupMenu();

	SaveUserConfig();
}

void CSbieView::SetCustomOrder()
{
	m_pSortProxy->sort(0, Qt::AscendingOrder);
	m_pSortProxy->setSortRole(Qt::InitialSortOrderRole);
	m_pSbieTree->header()->setSortIndicatorShown(false);
}

bool CSbieView::MoveItem(const QString& Name, const QString& To, int pos)
{
	QString From;

	// remove from old
	for (auto I = m_Groups.begin(); I != m_Groups.end(); ++I) {
		if (I.value().removeAll(Name))
			From = I.key();
	}

	// add to new
	m_Groups[To].insert(pos, Name);

	return From != To;
}

QString CSbieView::AddNewBox()
{
	CNewBoxWindow NewBoxWindow(this);
	bool bAlwaysOnTop = theConf->GetBool("Options/AlwaysOnTop", false);
	NewBoxWindow.setWindowFlag(Qt::WindowStaysOnTopHint, bAlwaysOnTop);
	if (NewBoxWindow.exec() == 1)
	{
		theAPI->ReloadBoxes();
		Refresh();
		SelectBox(NewBoxWindow.m_Name);
		return NewBoxWindow.m_Name;
	}
	return QString();
}

QString CSbieView::AddNewGroup()
{
	QString Name = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter a new group name"), QLineEdit::Normal);
	if (Name.isEmpty() || m_Groups.contains(Name))
		return "";
	if (!TestNameAndWarn(Name))
		return "";

	m_Groups[Name] = QStringList();

	QModelIndex ModelIndex = m_pSortProxy->mapToSource(m_pSbieTree->currentIndex());
	QString Parent;
	if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eGroup)
		Parent = m_pSbieModel->GetID(ModelIndex).toString();

	m_Groups[Parent].append(Name);

	
	//m_UserConfigChanged = true;
	UpdateGroupMenu();

	SaveUserConfig();

	return Name;
}

bool CSbieView::TestNameAndWarn(const QString& Name)
{
	if (m_Groups.contains(Name)) {
		QMessageBox::critical(this, "Sandboxie-Plus", tr("This name is already used for a Box Group."));
		return false;
	}

	if (!theAPI->GetBoxByName(QString(Name).replace(" ", "_")).isNull()) {
		QMessageBox::critical(this, "Sandboxie-Plus", tr("This name is already used for a Sandbox."));
		return false;
	}

	return true;
}

void CSbieView::OnSandBoxAction()
{
	OnSandBoxAction(qobject_cast<QAction*>(sender()));
}

void CSbieView::OnSandBoxAction(QAction* Action)
{
	QList<SB_STATUS> Results;

	QList<CSandBoxPtr> SandBoxes = CSbieView::GetSelectedBoxes();
	if (SandBoxes.isEmpty())
		return;
	if (Action == m_pStopAsync)
	{
		foreach(const CSandBoxPtr & pBox, SandBoxes)
		{
			auto pBoxEx = pBox.objectCast<CSandBoxPlus>();
			pBoxEx->OnCancelAsync();
		}
	}
	else if (Action == m_pMenuRunAny)
	{
		/*QString Command = ShowRunDialog(SandBoxes.first()->GetName());
		if(!Command.isEmpty())
			SandBoxes.first()->RunCommand(Command);*/

		Results.append(SandBoxes.first()->RunStart("run_dialog"));
	}
	else if (Action == m_pMenuRunMenu)
		Results.append(SandBoxes.first()->RunStart("start_menu"));
	else if (Action == m_pMenuRunBrowser)
		Results.append(SandBoxes.first()->RunStart("default_browser"));
	else if (Action == m_pMenuRunMailer)
		Results.append(SandBoxes.first()->RunStart("mail_agent"));
	else if (Action == m_pMenuRunExplorer)
	{
		if (theConf->GetInt("Options/ViewMode", 1) != 1 && theConf->GetBool("Options/BoxedExplorerInfo", true))
		{
			bool State = false;
			CCheckableMessageBox::question(this, "Sandboxie-Plus",
				theAPI->GetSbieMsgStr(0x00000DCDL, theGUI->m_LanguageId) // MSG_3533
				, tr("Don't show this message again."), &State, QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Information);

			if (State)
				theConf->SetValue("Options/BoxedExplorerInfo", false);
		}

		Results.append(SandBoxes.first()->RunStart("explorer.exe /e,::{20D04FE0-3AEA-1069-A2D8-08002B30309D}"));
	}
	else if (Action == m_pMenuRunRegEdit)
		Results.append(SandBoxes.first()->RunStart("regedit.exe"));
	else if (Action == m_pMenuRunAppWiz)
		Results.append(SandBoxes.first()->RunStart("\"C:\\WINDOWS\\System32\\control.exe\" \"C:\\Windows\\System32\\appwiz.cpl\""));
	else if (Action == m_pMenuAutoRun)
		Results.append(SandBoxes.first()->RunStart("auto_run"));
	else if (Action == m_pMenuRunCmd)
		Results.append(SandBoxes.first()->RunStart("cmd.exe"));
	else if (Action == m_pMenuRunCmdAdmin)
		Results.append(SandBoxes.first()->RunStart("cmd.exe", true));
	else if (Action == m_pMenuRunCmd32)
		Results.append(SandBoxes.first()->RunStart("C:\\WINDOWS\\SysWOW64\\cmd.exe"));
	else if (Action == m_pMenuPresetsShowUAC)
	{
		SandBoxes.first()->SetBool("DropAdminRights", false);
		SandBoxes.first()->SetBool("FakeAdminRights", false);
	}
	else if (Action == m_pMenuPresetsNoAdmin)
	{
		SandBoxes.first()->SetBool("DropAdminRights", true);
		SandBoxes.first()->SetBool("FakeAdminRights", false);
	}
	else if (Action == m_pMenuPresetsFakeAdmin)
	{
		SandBoxes.first()->SetBool("DropAdminRights", true);
		SandBoxes.first()->SetBool("FakeAdminRights", true);
	}
	else if (Action == m_pMenuPresetsINet)
		SandBoxes.first().objectCast<CSandBoxPlus>()->SetINetBlock(m_pMenuPresetsINet->isChecked());
	else if (Action == m_pMenuPresetsShares)
		SandBoxes.first().objectCast<CSandBoxPlus>()->SetAllowShares(m_pMenuPresetsShares->isChecked());
	else if (Action == m_pMenuPresetsRecovery)
		m_pMenuPresetsRecovery->setChecked(SandBoxes.first()->SetBool("AutoRecover", m_pMenuPresetsRecovery->isChecked()));
	else if (Action == m_pMenuOptions)
		ShowOptions(SandBoxes.first());
	else if (Action == m_pMenuBrowse)
		ShowBrowse(SandBoxes.first());
	else if (Action == m_pMenuRefresh)
	{
		foreach(const CSandBoxPtr& pBox, SandBoxes)
		{
			pBox.objectCast<CSandBoxPlus>()->UpdateSize();
		}	
	}
	else if (Action == m_pMenuExplore)
	{
		if (SandBoxes.first()->IsEmpty()) {
			QMessageBox("Sandboxie-Plus", tr("This Sandbox is empty."), QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
			return;
		}

		if (theConf->GetInt("Options/ViewMode", 1) != 1 && theConf->GetBool("Options/ExplorerInfo", true))
		{
			bool State = false;
			CCheckableMessageBox::question(this, "Sandboxie-Plus",
				theAPI->GetSbieMsgStr(0x00000DCEL, theGUI->m_LanguageId) // MSG_3534
				, tr("Don't show this message again."), &State, QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Information);

			if (State)
				theConf->SetValue("Options/ExplorerInfo", false);
		}

		::ShellExecute(NULL, NULL, SandBoxes.first()->GetFileRoot().toStdWString().c_str(), NULL, NULL, SW_SHOWNORMAL);
	}
	else if (Action == m_pMenuRegEdit)
	{
		if (SandBoxes.first()->IsEmpty()) {
			QMessageBox("Sandboxie-Plus", tr("This Sandbox is empty."), QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
			return;
		}

 		if (theConf->GetInt("Options/WarnOpenRegistry", -1) == -1)
		{
			bool State = false;
			if (CCheckableMessageBox::question(this, "Sandboxie-Plus", tr("WARNING: The opened registry editor is not sandboxed, please be careful and only do changes to the pre-selected sandbox locations.")
			  , tr("Don't show this warning in future"), &State, QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Yes, QMessageBox::Information) != QDialogButtonBox::Ok)
				return;

			if (State)
				theConf->SetValue("Options/WarnOpenRegistry", 1);
		}

		wstring path = QCoreApplication::applicationFilePath().toStdWString();

		QStringList RegRoot = SandBoxes.first()->GetRegRoot().split("\\");
		while (RegRoot.first().isEmpty())
			RegRoot.removeFirst();
		RegRoot[0] = QString("Computer");
		if(RegRoot[1] == "USER")
			RegRoot[1] = QString("HKEY_USERS");
		else if(RegRoot[1] == "MACHINE")
			RegRoot[1] = QString("HKEY_LOCAL_MACHINE");
		wstring params = L"-open_reg \"" + RegRoot.join("\\").toStdWString() + L"\"";
		if (SandBoxes.first()->GetActiveProcessCount() == 0)
			params += L" \"" + theAPI->GetStartPath().toStdWString() + L" /box:" + SandBoxes.first()->GetName().toStdWString() + L" mount_hive\"";

		SHELLEXECUTEINFO shex;
		memset(&shex, 0, sizeof(SHELLEXECUTEINFO));
		shex.cbSize = sizeof(SHELLEXECUTEINFO);
		shex.fMask = SEE_MASK_FLAG_NO_UI;
		shex.hwnd = NULL;
		shex.lpFile = path.c_str();
		shex.lpParameters = params.c_str();
		shex.nShow = SW_SHOWNORMAL;
		shex.lpVerb = L"runas";

		ShellExecuteEx(&shex);
	}
	else if (Action == m_pMenuSnapshots)
	{
		CSandBoxPtr pBox = SandBoxes.first();

		static QMap<void*, CSnapshotsWindow*> SnapshotWindows;
		CSnapshotsWindow* pSnapshotsWindow = SnapshotWindows.value(pBox.data());
		if (pSnapshotsWindow == NULL) {
			pSnapshotsWindow = new CSnapshotsWindow(SandBoxes.first(), this);
			SnapshotWindows.insert(pBox.data(), pSnapshotsWindow);
			connect(pSnapshotsWindow, &CSnapshotsWindow::Closed, [this, pBox]() {
				SnapshotWindows.remove(pBox.data());
			});
			SafeShow(pSnapshotsWindow);
		}
		else {
			pSnapshotsWindow->setWindowState((pSnapshotsWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
			SetForegroundWindow((HWND)pSnapshotsWindow->winId());
		}
	}
	else if (Action == m_pMenuDuplicate)
	{
		QString OldValue = SandBoxes.first()->GetName().replace("_", " ");
		QString Value = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter a new name for the duplicated Sandbox."), QLineEdit::Normal, tr("%1 Copy").arg(OldValue));
		if (Value.isEmpty() || Value == OldValue)
			return;
		
		QString Name = Value.replace(" ", "_");
		SB_STATUS Status = theAPI->CreateBox(Name, false);

		if (!Status.IsError())
		{
			CSandBoxPtr pBox = theAPI->GetBoxByName(Value);

			QList<QPair<QString, QString>> Settings;
			CSandBoxPtr pSrcBox = theAPI->GetBoxByName(SandBoxes.first()->GetName());
			qint32 status = 0;
			if (!pSrcBox.isNull()) Settings = pSrcBox->GetIniSection(&status);
			if (Settings.isEmpty())
				Status = SB_ERR(SB_FailedCopyConf, QVariantList() << SandBoxes.first()->GetName() << (quint32)status);
			else
			{
				for (QList<QPair<QString, QString>>::iterator I = Settings.begin(); I != Settings.end(); ++I)
				{
					if (I->first == "FileRootPath" && !I->second.toUpper().contains("%SANDBOX%"))
						continue; // skip the FileRootPath if it does not contain a %SANDBOX% 

					Status = theAPI->SbieIniSet(Name, I->first, I->second, CSbieAPI::eIniInsert, false);
					if (Status.IsError())
						break;
				}
			}

			theAPI->CommitIniChanges();
			theAPI->ReloadConfig();
			theAPI->ReloadBoxes();
		}

		Results.append(Status);
	}
	else if (Action == m_pMenuRename)
	{
		QString OldValue = SandBoxes.first()->GetName().replace("_", " ");
		QString Value = QInputDialog::getText(this, "Sandboxie-Plus", tr("Please enter a new name for the Sandbox."), QLineEdit::Normal, OldValue);
		if (Value.isEmpty() || Value == OldValue)
			return;
		if (!TestNameAndWarn(Value))
			return;

		Results.append((SandBoxes.first()->RenameBox(Value.replace(" ", "_"))));

		RenameItem(OldValue, Value);
	}
	else if (Action == m_pMenuRecover)
	{
		theGUI->ShowRecovery(SandBoxes.first());
	}
	else if (Action == m_pMenuRemove)
	{
		if (QMessageBox("Sandboxie-Plus", tr("Do you really want to remove the selected sandbox(es)?<br /><br />Warning: The box content will also be deleted!"), QMessageBox::Warning, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton, this).exec() != QMessageBox::Yes)
			return;

		foreach(const CSandBoxPtr& pBox, SandBoxes)
		{
			SB_STATUS Status = theGUI->DeleteBoxContent(pBox, CSandMan::eForDelete);
			if (Status.GetMsgCode() == SB_Canceled)
				break;
			if (!Status.IsError())
				Status = pBox->RemoveBox();
			Results.append(Status);
		}	
	}
	else if (Action == m_pMenuCleanUp)
	{
		bool DeleteShapshots = false;

		if (SandBoxes.count() == 1)
		{
			if (SandBoxes.first()->IsEmpty()) {
				QMessageBox("Sandboxie-Plus", tr("This Sandbox is already empty."), QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
				return;
			}

			if (theConf->GetBool("Options/ShowRecovery", false))
			{
				// Use recovery dialog in place of the confirmation messagebox for box clean up
				if(!theGUI->OpenRecovery(SandBoxes.first(), DeleteShapshots))
					return;
			}
			else if(CCheckableMessageBox::question(this, "Sandboxie-Plus", tr("Do you want to delete the content of the selected sandbox?")
				, tr("Also delete all Snapshots"), &DeleteShapshots, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) != QDialogButtonBox::Yes)
					return;
		}
		else if(CCheckableMessageBox::question(this, "Sandboxie-Plus", tr("Do you really want to delete the content of all selected sandboxes?")
			, tr("Also delete all Snapshots"), &DeleteShapshots, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) != QDialogButtonBox::Yes)
				return;

		foreach(const CSandBoxPtr &pBox, SandBoxes)
		{
			if (theConf->GetBool("Options/UseAsyncBoxOps", false))
			{
				auto pBoxEx = pBox.objectCast<CSandBoxPlus>();
				SB_STATUS Status = pBoxEx->DeleteContentAsync(DeleteShapshots);
				if (Status.IsError())
					Results.append(Status);
			}
			else  
			{
				SB_STATUS Status = theGUI->DeleteBoxContent(pBox, CSandMan::eDefault, DeleteShapshots);
				if (Status.GetMsgCode() == SB_Canceled)
					break;
				Results.append(Status);
			}
		}	
	}
	else if (Action == m_pMenuEmptyBox)
	{
 		if (theConf->GetInt("Options/WarnTerminate", -1) == -1)
		{
			bool State = false;
			if(CCheckableMessageBox::question(this, "Sandboxie-Plus",  tr("Do you want to terminate all processes in the selected sandbox(es)?")
				, tr("Terminate without asking"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) != QDialogButtonBox::Yes)
				return;

			if (State)
				theConf->SetValue("Options/WarnTerminate", 1);
		}

		foreach(const CSandBoxPtr& pBox, SandBoxes)
			Results.append(pBox->TerminateAll());
	}
	else if (Action == m_pMenuMkLink)
	{
 		if (theConf->GetInt("Options/InfoMkLink", -1) == -1)
		{
			bool State = false;
			CCheckableMessageBox::question(this, "Sandboxie-Plus", tr("The Sandboxie Start Menu will now be displayed. Select an application from the menu, and Sandboxie will create a new"
				"shortcut icon on your real desktop, which you can use to invoke the selected application under the supervision of Sandboxie.")
				, tr("Don't show this message again."), &State, QDialogButtonBox::Ok, QDialogButtonBox::Ok, QMessageBox::Information);
			if (State)
				theConf->SetValue("Options/InfoMkLink", 1);
		}

		QString BoxName = SandBoxes.first()->GetName();
		QString LinkPath, IconPath, WorkDir;
		quint32 IconIndex;
		if (!CSbieUtils::GetStartMenuShortcut(theAPI, BoxName, LinkPath, IconPath, IconIndex, WorkDir))
			return;
		
		QString LinkName;
		int pos = LinkPath.lastIndexOf(L'\\');
		if (pos == -1)
			return;
		if (pos == 2 && LinkPath.length() == 3)
			LinkName = QObject::tr("Drive %1").arg(LinkPath.left(1));
		else {
			LinkName = LinkPath.mid(pos + 1);
			pos = LinkName.indexOf(QRegExp("[" + QRegExp::escape("\":;,*?.") + "]"));
			if (pos != -1)
				LinkName = LinkName.left(pos);
		}

		QString Path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).replace("/", "\\");
		//Path = QFileDialog::getExistingDirectory(this, tr("Select Directory to create Shorcut in"), Path).replace("/", "\\");
		//if (Path.isEmpty())
		//	return;

		if (Path.right(1) != "\\")
			Path.append("\\");
		Path += "[" + BoxName + "] " + LinkName;

		Path = QFileDialog::getSaveFileName(this, tr("Create Shortcut to sandbox %1").arg(BoxName), Path, QString("Shortcut files (*.lnk)")).replace("/", "\\");
		if (Path.isEmpty())
			return;

		CSbieUtils::CreateShortcut(theAPI, Path, LinkName, BoxName, LinkPath, IconPath, IconIndex, WorkDir);
	}
	else // custom run menu command
	{
		QString Command = Action->data().toString();
		if (!Command.isEmpty())
		{
			if (Command.left(1) == "\\" && !SandBoxes.isEmpty())
				Command.prepend(SandBoxes.first()->GetFileRoot());
			Results.append(SandBoxes.first()->RunStart(Command));
		}
	}

	CSandMan::CheckResults(Results);
}

void CSbieView::OnProcessAction()
{
	OnProcessAction(qobject_cast<QAction*>(sender()));
}

void CSbieView::OnProcessAction(QAction* Action)
{
	QList<SB_STATUS> Results;

	QList<CBoxedProcessPtr> Processes = CSbieView::GetSelectedProcesses();

	if (Action == m_pMenuTerminate || Action == m_pMenuBlackList)
	{
 		if (theConf->GetInt("Options/WarnTerminate", -1) == -1)
		{
			if (Processes.isEmpty())
				return;

			bool State = false;
			if(CCheckableMessageBox::question(this, "Sandboxie-Plus", tr("Do you want to %1 %2?").arg(((QAction*)sender())->text().toLower()).arg(Processes.count() == 1 ? Processes[0]->GetProcessName() : tr("the selected processes"))
				, tr("Terminate without asking"), &State, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes) != QDialogButtonBox::Yes)
				return;

			if (State)
				theConf->SetValue("Options/WarnTerminate", 1);
		}
	}

	foreach(const CBoxedProcessPtr& pProcess, Processes)
	{
		if (Action == m_pMenuTerminate)
			Results.append(pProcess->Terminate());
		else if (Action == m_pMenuLinkTo)
		{
			QString BoxName = pProcess->GetBoxName();
			QString LinkName = pProcess->GetProcessName();
			QString LinkPath = pProcess->GetFileName();

			QString Path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).replace("/", "\\");
			//Path = QFileDialog::getExistingDirectory(this, tr("Select Directory to create Shorcut in"), Path).replace("/", "\\");
			//if (Path.isEmpty())
			//	return;

			if (Path.right(1) != "\\")
				Path.append("\\");
			Path += "[" + BoxName + "] " + LinkName;

			Path = QFileDialog::getSaveFileName(this, tr("Create Shortcut to sandbox %1").arg(BoxName), Path, QString("Shortcut files (*.lnk)")).replace("/", "\\");
			if (Path.isEmpty())
				return;

			CSbieUtils::CreateShortcut(theAPI, Path, LinkName, BoxName, LinkPath, LinkPath);
		}
		else if (Action == m_pMenuPinToRun)
		{
			CSandBoxPlus* pBoxPlus = pProcess.objectCast<CSbieProcess>()->GetBox();

			if (m_pMenuPinToRun->isChecked())
			{
				QString FileName = pProcess->GetFileName();
				if (FileName.indexOf(pBoxPlus->GetFileRoot(), Qt::CaseInsensitive) == 0) {
					FileName.remove(0, pBoxPlus->GetFileRoot().length());
					if (FileName.at(0) != "\\")
						FileName.prepend("\\");
				}

				pBoxPlus->InsertText("RunCommand", pProcess->GetProcessName() + "|\"" + pProcess->GetFileName()+"\"");
			}
			else if(!m_pMenuPinToRun->data().toString().isEmpty())
				pBoxPlus->DelValue("RunCommand", m_pMenuPinToRun->data().toString());
		}
		else if (Action == m_pMenuBlackList)
		{
			Results.append(pProcess->Terminate());
			pProcess.objectCast<CSbieProcess>()->BlockProgram();
		}
		else if (Action == m_pMenuAllowInternet)
		{
			if (!pProcess.objectCast<CSbieProcess>()->GetBox()->IsINetBlocked())
			{
				if (QMessageBox("Sandboxie-Plus", tr("This box does not have Internet restrictions in place, do you want to enable them?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton, this).exec() != QMessageBox::Yes)
					return;
				pProcess.objectCast<CSbieProcess>()->GetBox()->SetINetBlock(true);
			}
			pProcess.objectCast<CSbieProcess>()->SetInternetAccess(m_pMenuAllowInternet->isChecked());
		}
		else if (Action == m_pMenuMarkForced)
			pProcess.objectCast<CSbieProcess>()->SetForcedProgram(m_pMenuMarkForced->isChecked());
		else if (Action == m_pMenuMarkLinger)
			pProcess.objectCast<CSbieProcess>()->SetLingeringProgram(m_pMenuMarkLinger->isChecked());
		else if (Action == m_pMenuMarkLeader)
			pProcess.objectCast<CSbieProcess>()->SetLeaderProgram(m_pMenuMarkLeader->isChecked());
		/*else if (Action == m_pMenuSuspend)
			Results.append(pProcess->SetSuspend(true));
		else if (Action == m_pMenuResume)
			Results.append(pProcess->SetSuspend(false));*/
	}

	CSandMan::CheckResults(Results);
}

void CSbieView::ShowOptions(const CSandBoxPtr& pBox)
{
	auto pBoxEx = pBox.objectCast<CSandBoxPlus>();
	if (pBoxEx->m_pOptionsWnd == NULL) {
		pBoxEx->m_pOptionsWnd = new COptionsWindow(pBox, pBox->GetName());
		connect(pBoxEx->m_pOptionsWnd, &COptionsWindow::Closed, [pBoxEx]() {
			pBoxEx->m_pOptionsWnd = NULL;
		});
		SafeShow(pBoxEx->m_pOptionsWnd);
	}
	else {
		pBoxEx->m_pOptionsWnd->setWindowState((pBoxEx->m_pOptionsWnd->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
		SetForegroundWindow((HWND)pBoxEx->m_pOptionsWnd->winId());
	}
}

void CSbieView::ShowBrowse(const CSandBoxPtr& pBox)
{
	if (pBox->IsEmpty()) {
		QMessageBox("Sandboxie-Plus", tr("This Sandbox is empty."), QMessageBox::Information, QMessageBox::Ok, QMessageBox::NoButton, QMessageBox::NoButton, this).exec();
		return;
	}

	static QMap<void*, CFileBrowserWindow*> FileBrowserWindows;
	CFileBrowserWindow* pFileBrowserWindow = FileBrowserWindows.value(pBox.data());
	if (pFileBrowserWindow == NULL) {
		pFileBrowserWindow = new CFileBrowserWindow(pBox);
		FileBrowserWindows.insert(pBox.data(), pFileBrowserWindow);
		connect(pFileBrowserWindow, &CFileBrowserWindow::Closed, [this, pBox]() {
			FileBrowserWindows.remove(pBox.data());
		});
		SafeShow(pFileBrowserWindow);
	}
	else {
		pFileBrowserWindow->setWindowState((pFileBrowserWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
		SetForegroundWindow((HWND)pFileBrowserWindow->winId());
	}
}

void CSbieView::OnDoubleClicked(const QModelIndex& index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);
	CSandBoxPtr pBox = m_pSbieModel->GetSandBox(ModelIndex);
	if (pBox.isNull())
		return;

	if ((QGuiApplication::queryKeyboardModifiers() & Qt::ControlModifier) == 0) {

		if (index.column() == CSbieModel::ePath) {
			OnSandBoxAction(m_pMenuExplore);
			return;
		}
	}

	//if (index.column() != CSbieModel::eName)
	//	return;

	if (!pBox->IsEnabled())
	{
		if (QMessageBox("Sandboxie-Plus", tr("This sandbox is disabled, do you want to enable it?"), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton, this).exec() != QMessageBox::Yes)
			return;
		pBox->SetText("Enabled", "y");
		return;
	}
	
	
	ShowOptions(pBox);
}

void CSbieView::ProcessSelection(const QItemSelection& selected, const QItemSelection& deselected)
{
	if (selected.empty()) {
		//UpdateMenu();
		return;
	}

	QItemSelectionModel* selectionModel = m_pSbieTree->selectionModel();
	QItemSelection selection = selectionModel->selection();
	QItemSelection invalid;

	/*
	QModelIndex root_parent = m_pSbieTree->currentIndex().parent();
	while (root_parent.isValid() && root_parent.parent().isValid())
		root_parent = root_parent.parent();

	foreach(const QModelIndex& index, selection.indexes())
	{
		QModelIndex parent = index.parent();
		while (parent.isValid() && parent.parent().isValid())
			parent = parent.parent();

		if (parent != root_parent)
			invalid.select(index, index);
	}*/

	int Type = m_pSbieModel->GetType(m_pSortProxy->mapToSource(m_pSbieTree->currentIndex()));

	foreach(const QModelIndex& index, selection.indexes())
	{
		if (m_pSbieModel->GetType(m_pSortProxy->mapToSource(index)) != Type)
			invalid.select(index, index);
	}

	selectionModel->select(invalid, QItemSelectionModel::Deselect);

	//UpdateMenu();
}

QList<CSandBoxPtr> CSbieView::GetSelectedBoxes()
{
	QList<CSandBoxPtr> List;
	foreach(const QModelIndex& Index, m_pSbieTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CSandBoxPtr pBox = m_pSbieModel->GetSandBox(ModelIndex);
		if (!pBox)
			continue;
		List.append(pBox);
	}
	return  List;
}

QList<CBoxedProcessPtr> CSbieView::GetSelectedProcesses()
{
	QList<CBoxedProcessPtr> List;
	foreach(const QModelIndex& Index, m_pSbieTree->selectedRows())
	{
		QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
		CBoxedProcessPtr pProcess = m_pSbieModel->GetProcess(ModelIndex);
		if (!pProcess)
			return QList < CBoxedProcessPtr>();
		List.append(pProcess);
	}
	return  List;
}

void CSbieView::UpdateRunMenu(const CSandBoxPtr& pBox)
{
	while (m_iMenuRun < m_pMenuRun->actions().count())
		m_pMenuRun->removeAction(m_pMenuRun->actions().at(m_iMenuRun));

	QStringList RunOptions = pBox->GetTextList("RunCommand", true);
	foreach(const QString& RunOption, RunOptions) 
	{
		StrPair NameCmd = Split2(RunOption, "|");
		QAction* pAction = m_pMenuRun->addAction(NameCmd.first, this, SLOT(OnSandBoxAction()));

		QString Path = NameCmd.second;
		if (Path.left(1) == "\"") {
			int End = Path.indexOf("\"", 1);
			if (End != -1) Path = Path.mid(1, End - 1);
		}
		else {
			int End = Path.indexOf(" ");
			if (End != -1) Path.truncate(End);
		}

		if (Path.left(1) == "\\")
			Path.prepend(pBox->GetFileRoot());

		pAction->setIcon(m_IconProvider.icon(QFileInfo(Path)));

		pAction->setData(NameCmd.second);
	}
}

void CSbieView::SelectBox(const QString& Name)
{
	if(m_pSbieModel->Count() == 0)
		Refresh();

	QModelIndex Index = m_pSbieModel->FindIndex(Name);
	QModelIndex ModelIndex = m_pSortProxy->mapFromSource(Index);

	QModelIndex ModelL = m_pSortProxy->index(ModelIndex.row(), 0, ModelIndex.parent());
	QModelIndex ModelR = m_pSortProxy->index(ModelIndex.row(), m_pSortProxy->columnCount() - 1, ModelIndex.parent());

	QItemSelection SelectedItems;
	SelectedItems.append(QItemSelectionRange(ModelL, ModelR));

	m_pSbieTree->setCurrentIndex(ModelIndex);
	m_pSbieTree->scrollTo(ModelL);
	m_pSbieTree->selectionModel()->select(SelectedItems, QItemSelectionModel::ClearAndSelect);
}

void CSbieView::PopUpMenu(const QString& Name)
{
	//SelectBox(Name);
	CSandBoxPtr pBox = theAPI->GetBoxByName(Name);
	if (pBox.isNull() || !UpdateMenu(false, pBox)) return;
	m_pMenu2->exec(QCursor::pos());
	//m_pMenu2->popup(QCursor::pos());
	//OnMenu(QCursor::pos());
}

QMenu* CSbieView::GetMenu(const QString& Name)
{
	CSandBoxPtr pBox = theAPI->GetBoxByName(Name);
	if (pBox.isNull()) return NULL;
	UpdateMenu(false, pBox);
	return m_pMenu;
}

void CSbieView::ShowOptions(const QString& Name)
{
	QModelIndex Index = m_pSbieModel->FindIndex(Name);
	QModelIndex ModelIndex = m_pSortProxy->mapFromSource(Index);

	OnDoubleClicked(ModelIndex);
}

void CSbieView::ChangeExpand(const QModelIndex& index, bool bExpand)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(index);

	if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eProcess)
		return;

	QString Name;
	if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eGroup)
		Name = m_pSbieModel->GetID(ModelIndex).toString();
	else if (m_pSbieModel->GetType(ModelIndex) == CSbieModel::eBox)
		Name = m_pSbieModel->GetSandBox(ModelIndex)->GetName();

	if(bExpand)
		m_Collapsed.remove(Name);
	else
		m_Collapsed.insert(Name);

	//m_UserConfigChanged = true;

	SaveUserConfig();
}

void CSbieView::ReloadUserConfig()
{
	m_Groups.clear();

	QString Grouping = theConf->GetString("UIConfig/BoxDisplayOrder");
	if(Grouping.isEmpty())
		Grouping = theAPI->GetUserSettings()->GetText("BoxDisplayOrder");
	CSbieView__ParseGroup(Grouping, m_Groups);

	UpdateGroupMenu();

	QString Collapsed = theConf->GetString("UIConfig/BoxCollapsedView");
	if (Collapsed.isEmpty())
		Collapsed = theAPI->GetUserSettings()->GetText("BoxCollapsedView");
	m_Collapsed = SplitStr(Collapsed, ",").toSet();
}

void CSbieView::SaveUserConfig()
{
	//if (!m_UserConfigChanged)
	//	return;
	//m_UserConfigChanged = false;

	QString Grouping = CSbieView__SerializeGroup(m_Groups);
	theConf->SetValue("UIConfig/BoxDisplayOrder", Grouping);

	QString Collapsed = m_Collapsed.toList().join(",");
	theConf->SetValue("UIConfig/BoxCollapsedView", Collapsed);
}

void CSbieView::OnMoveItem(const QString& Name, const QString& To, int row)
{
	QModelIndex index;
	if (!To.isEmpty()) { // only groups can be parents so add the group marker "!"
		QModelIndex index0 = m_pSbieModel->FindIndex("!" + To);
		index = index0.child(row, 0);	
	} else
		index = m_pSbieModel->index(row, 0);
	QModelIndex index2 = m_pSortProxy->mapFromSource(index);
	int row2 = index2.row();
	if (MoveItem(Name, To, row2)) {
		m_pSbieModel->Clear(); //todo improve that
		Refresh();
	}

	//m_UserConfigChanged = true;
	UpdateGroupMenu();

	SaveUserConfig();
}

void CSbieView::OnRemoveItem() 
{
	if (!GetSelectedProcesses().isEmpty())
		OnProcessAction(m_pMenuTerminate);
	else if (!GetSelectedBoxes().isEmpty())
		OnSandBoxAction(m_pMenuRemove);
	else if (!GetSelectedGroups().isEmpty())
		OnGroupAction(m_pDelGroupe);
}
