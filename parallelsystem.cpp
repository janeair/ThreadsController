#include "parallelsystem.h"

parallelsystem::parallelsystem(QWidget *parent)
: QMainWindow(parent)
{
	init();
	qRegisterMetaType<ThreadState>("ThreadState");
	// BUTTONS
	connect(StartButton, &QPushButton::clicked, this, &parallelsystem::changeState);
	connect(AddButton, &QPushButton::clicked, this, &parallelsystem::addThreadManual);
	connect(RemoveButton, &QPushButton::clicked, this, &parallelsystem::removeThreadManual);
	// TASK MANAGER
	connect(MyTaskManager, &TaskManager::finishTime, this, &parallelsystem::finishTask);
	connect(MyTaskManager, &TaskManager::finishThread, BarThreadChart, &BarChartView::addFinishedTask);
	// LOAD SYSTEM
	connect(System, &LoadControl::addThread, this, &parallelsystem::addThread);
	connect(System, &LoadControl::removeThread, this, &parallelsystem::removeThread);
	// SCALE CHART
	connect(System, &LoadControl::timeDataChanged, StarScaleChart, &StarChartView::addScalePoint);
	connect(StarScaleChart, &StarChartView::changedScalePoint, System, &LoadControl::setTimeData);
	// BAR CHART + LOAD CHART
	connect(LoadChart, &LoadChartView::updatePerformance, BarThreadChart, &BarChartView::makeOverallPerformance);
	connect(BarThreadChart, &BarChartView::sendOverallPerformance, LoadChart, &LoadChartView::addPerformancePoint);
	// SYSTEM CONTROL CHECK BOX
	connect(SystemControlBox, &QCheckBox::stateChanged, this, &parallelsystem::changeSystemState);
}


parallelsystem::~parallelsystem()
{

}


void parallelsystem::init() // building the GUI
{
	this->setMinimumSize(QSize(500, 500));
	this->resize(QSize(500, 500));
	this->setWindowTitle("Load System");

	PerfectThreadCount = QThread::idealThreadCount();

	StartButton = new QPushButton(QString("Start"));
	StartButton->setStyleSheet("font: 8pt Tahoma;");
	AddButton = new QPushButton(QString("Add Thread"));
	AddButton->setStyleSheet("font: 8pt Tahoma;");
	RemoveButton = new QPushButton(QString("Remove Thread"));
	RemoveButton->setStyleSheet("font: 8pt Tahoma;");

	InfoEdit = new QTextEdit();
	InfoEdit->setStyleSheet("font: bold 8pt Tahoma;");
	InfoEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	InfoEdit->setMaximumHeight(100);

	ThreadNumberBox = new QSpinBox();
	ThreadNumberBox->setStyleSheet("font: bold 8pt Tahoma;");
	ThreadNumberBox->setAlignment(Qt::AlignCenter);
	ThreadNumberBox->setRange(1, PerfectThreadCount + OVERLOAD);

	LoadChart = new LoadChartView(PerfectThreadCount + OVERLOAD, this);

	StarScaleChart = new StarChartView(PerfectThreadCount + OVERLOAD, this);

	BarThreadChart = new BarChartView(this, PerfectThreadCount + OVERLOAD);

	// creating a new separate window for star scale chart

	MinorWindow* newWindow0 = new MinorWindow(StarScaleChart, QSize(400, 400), this);
	connect(this, SIGNAL(closed()), newWindow0, SLOT(close()));
	
	StarDisplayBox = new WindowControlCheckBox(newWindow0, "Scale Chart");
	StarDisplayBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	StarDisplayBox->setMaximumSize(QSize(100, 60));

	// creating a new separate window for bar load chart

	MinorWindow* newWindow1 = new MinorWindow(BarThreadChart, QSize(400, 400), this);
	connect(this, SIGNAL(closed()), newWindow1, SLOT(close()));

	BarDisplayBox = new WindowControlCheckBox(newWindow1, "Bar Chart");
	BarDisplayBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	BarDisplayBox->setMaximumSize(QSize(100, 50));

	// creating a system control check box

	SystemControlBox = new QCheckBox("System Control", this);
	SystemControlBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	SystemControlBox->setMaximumSize(QSize(200, 50));
	SystemControlBox->setStyleSheet("spacing: 8px; font: bold 7pt Tahoma;");
	SystemControlBox->setChecked(true);

	// PALETTE SETTINGS
	
	QPalette darkPalette;

	darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
	darkPalette.setColor(QPalette::WindowText, Qt::white);
	darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
	darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
	darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
	darkPalette.setColor(QPalette::ToolTipText, Qt::white);
	darkPalette.setColor(QPalette::Text, Qt::white);
	darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
	darkPalette.setColor(QPalette::ButtonText, Qt::white);
	darkPalette.setColor(QPalette::BrightText, Qt::red);
	darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
	darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
	darkPalette.setColor(QPalette::HighlightedText, Qt::black);
	
	qApp->setPalette(darkPalette);
	

	// LAYOUTS

	QHBoxLayout *chartboxlayout = new QHBoxLayout(this);
	chartboxlayout->addWidget(StarDisplayBox);
	chartboxlayout->addWidget(BarDisplayBox);
	chartboxlayout->setMargin(0);

	QHBoxLayout *systemboxlayout = new QHBoxLayout(this);
	systemboxlayout->addWidget(SystemControlBox);
	systemboxlayout->setMargin(0);

	QGroupBox *chartgroup = new QGroupBox("Charts", this);
	chartgroup->setLayout(chartboxlayout);
	chartgroup->setCheckable(true);
	chartgroup->setChecked(true);
	chartgroup->setContentsMargins(10, 25, 10, 5);

	QGroupBox *systemgroup = new QGroupBox("System", this);
	systemgroup->setLayout(systemboxlayout);
	systemgroup->setCheckable(true);
	systemgroup->setChecked(false);
	systemgroup->setContentsMargins(10, 25, 10, 5);

	QVBoxLayout *checkboxgrouplayout = new QVBoxLayout(this);
	checkboxgrouplayout->addWidget(chartgroup);
	checkboxgrouplayout->addWidget(systemgroup);
	checkboxgrouplayout->setMargin(0);
	
	QHBoxLayout *tophlayout = new QHBoxLayout(this);
	tophlayout->addWidget(StartButton);
	tophlayout->addWidget(AddButton);
	tophlayout->addWidget(RemoveButton);
	tophlayout->addWidget(ThreadNumberBox);
	tophlayout->setAlignment(Qt::AlignTop);

	QHBoxLayout *middlehlayout = new QHBoxLayout(this);
	middlehlayout->addWidget(LoadChart);

	QHBoxLayout *bottomhlayout = new QHBoxLayout(this);
	bottomhlayout->addWidget(InfoEdit);
	bottomhlayout->addLayout(checkboxgrouplayout);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->addLayout(tophlayout);
	layout->addLayout(middlehlayout);
	layout->addLayout(bottomhlayout);

	QWidget *CenterWidget = new QWidget();

	CenterWidget->setLayout(layout);
	setCentralWidget(CenterWidget);


	// PRIVATE OBJECTS

	MyTaskManager = new TaskManager(PerfectThreadCount);

	System = new LoadControl(PerfectThreadCount);

	IsRunning = false;


	// BUTTON SETTINGS

	AddButton->setEnabled(false);
	RemoveButton->setEnabled(false);

	// ICON SETTINGS

	QIcon icon = QIcon("Resources/main_icon.png");
	if (!icon.isNull()) { this->setWindowIcon(icon); }
	else { qDebug() << "parallelsystem: invalid pointer | couldn't set icon"; }
}


void parallelsystem::startThreads()
{
	AddButton->setEnabled(true);
	RemoveButton->setEnabled(true);
	ThreadNumberBox->setEnabled(false);
	StartButton->setText("Stop");
	InfoEdit->append("#start " + QString::number(ThreadNumberBox->value()));
	if (SystemControlBox->isChecked()) InfoEdit->append("#system switches on");
	MyTaskManager->startThreads(ThreadNumberBox->value());
	System->start(ThreadNumberBox->value());
	LoadChart->addPerformancePoint(0.0);
	LoadChart->addLoadPoint(ThreadNumberBox->value());
	LoadChart->setPerformanceAxisCalibrated(0);
	StarScaleChart->clearOverloadSeries();
	StarScaleChart->setRadialAxisCalibrated(0);
	BarThreadChart->clearChart();
	BarThreadChart->clearBase();
	BarThreadChart->setTaskAxisCalibrated(0);
}


void parallelsystem::stopThreads()
{
	AddButton->setEnabled(false);
	RemoveButton->setEnabled(false);
	ThreadNumberBox->setEnabled(true);
	StartButton->setText("Start");
	InfoEdit->append("#stop");
	if (SystemControlBox->isChecked()) InfoEdit->append("#system switches off");
	MyTaskManager->stopThreads();
	System->stop();
	LoadChart->addLoadPoint(0);
}


void parallelsystem::changeState()
{
	if (IsRunning == true)
	{
		IsRunning = false;
		stopThreads();
	}
	else
	{
		IsRunning = true;
		startThreads();
	}
}

void parallelsystem::changeSystemState(int state)
{
	if (System->changeState(state))
	{
		if (state == Qt::Checked)
			InfoEdit->append("#system switches on");
		else if (state == Qt::Unchecked)
			InfoEdit->append("#system switches off");
	}
}


void parallelsystem::addThread()
{
	if (ThreadNumberBox->value() < PerfectThreadCount + OVERLOAD)
	{
		ThreadNumberBox->setValue(ThreadNumberBox->value() + 1);
		InfoEdit->append("#change " + QString::number(ThreadNumberBox->value()));
		System->setThreadCount(ThreadNumberBox->value());
		MyTaskManager->addThread();
		System->updateSystemState(1);
		LoadChart->addLoadPoint(ThreadNumberBox->value());
	}
}


void parallelsystem::removeThread(qreal ms = 0.0)
{
	if (ThreadNumberBox->value() > 1)
	{
		ThreadNumberBox->setValue(ThreadNumberBox->value() - 1);
		InfoEdit->append("#change " + QString::number(ThreadNumberBox->value()));
		System->setThreadCount(ThreadNumberBox->value());
		MyTaskManager->removeThread();
		System->updateSystemState(-1);
		LoadChart->addLoadPoint(ThreadNumberBox->value());
		if (ms != 0.0) StarScaleChart->addOverloadPoint(ThreadNumberBox->value() + 1, ms);
	}
}


void parallelsystem::finishTask(int ms)
{
	// inform the control system about the completion of the task
	System->finishedTask(ms);
}



// TASK MANAGER

void TaskManager::addThread()
{
	if (CurrentThreadNumber < PerfectThreadCount + OVERLOAD)
	{
		MyThreadPool.setMaxThreadCount(CurrentThreadNumber + 1);
		CurrentThreadNumber++;
	}
}


void TaskManager::removeThread()
{
	if (CurrentThreadNumber > 1)
	{
		MyThreadPool.setMaxThreadCount(CurrentThreadNumber - 1);
		CurrentThreadNumber--;
	}
}


void TaskManager::setMaxThreadNumber(int num)
{
	if ((num >= 1) && (num < PerfectThreadCount + OVERLOAD + 1))
	{
		MyThreadPool.setMaxThreadCount(num);
		setCurrentThreadNumber(num);
	}
	else
	{
		MyThreadPool.setMaxThreadCount(1);
		setCurrentThreadNumber(0);
	}
}


void TaskManager::addTask()
{
	if (CurrentThreadNumber > 0)
	{
		ThreadTask* task = new ThreadTask(TaskCount);
		TaskCount++;
		if (TaskCount == SEARCH_RANGE + 1)
			TaskCount = -SEARCH_RANGE;
		
		connect(task, SIGNAL(finish(ThreadState)), this, SLOT(addTask()));
		connect(task, SIGNAL(finish(ThreadState)), this, SLOT(finishTask(ThreadState)));
		MyThreadPool.start(task);
	}
}


void TaskManager::finishTask(ThreadState thread_state)
{
	emit finishTime(thread_state.getTime());
	emit finishThread(thread_state);
}


void TaskManager::startThreads(int ThreadNumber)
{
	setMaxThreadNumber(ThreadNumber);
	TaskCount = -SEARCH_RANGE;

	for (int i = 0; i < PerfectThreadCount + OVERLOAD + 1; i++)
	{
		ThreadTask* task = new ThreadTask(TaskCount);
		TaskCount++;
		if (TaskCount == SEARCH_RANGE + 1)
			TaskCount = -SEARCH_RANGE;
		connect(task, SIGNAL(finish(ThreadState)), this, SLOT(addTask()));
		connect(task, SIGNAL(finish(ThreadState)), this, SLOT( finishTask(ThreadState)));
		MyThreadPool.start(task);
	}

}
