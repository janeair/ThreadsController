#ifndef PARALLELSYSTEM_H
#define PARALLELSYSTEM_H

#define OVERLOAD 0 // number of threads allowed over IdealThreadCount

#define K_CONST 588 // JUST FOR FUN
#define SEARCH_RANGE 5000 //

#include <QtWidgets/QMainWindow>
#include "threadbase.h"
#include <iostream>
#include <qdebug.h>
#include <qthreadpool.h>
#include <qpushbutton.h>
#include <qspinbox.h>
#include <QHBoxlayout>
#include <QVBoxlayout>
#include <qtime>
#include <QToolTip>
#include <qtextedit.h>
#include <QtCharts>
#include <qmenu>


// THREAD TASK CLASS - tasks executing by every thread

class ThreadTask : public QObject, public QRunnable
{
	Q_OBJECT

public:
	ThreadTask(qint64 num)
	{
		id = num;
	}

public slots:

	void run() // task to load only CPU
	{
		// setting priority options
		if (id == -SEARCH_RANGE) // the very first task
		{
			QThread::currentThread()->setPriority(QThread::LowestPriority); // set the thread's taken first task priority to lowest (only this one)
		}
		else
		{
			if (QThread::currentThread()->priority() == QThread::InheritPriority)
			{
				QThread::currentThread()->setPriority(QThread::NormalPriority);
			}
		}

		QTime timer;
		timer.start();
		for (qint64 j = -SEARCH_RANGE; j <= SEARCH_RANGE; j++)
		for (qint64 k = -SEARCH_RANGE; k <= SEARCH_RANGE; k++)
		{
			if (id*id*id + j*j*j + k*k*k == K_CONST)
				qDebug() << "found" << id << j << k
			;
		}

		// gathering information about thread state
		ThreadState thread_state = ThreadState(QString("0x%1").arg((uint)QThread::currentThreadId(), 4, 16, QLatin1Char('0')), QThread::currentThread(), timer.elapsed(), QThread::currentThread()->priority());
		emit finish(thread_state);
	}

signals:
	void finish(ThreadState thread_state);

private:
	qint64 id = 0; // task id
};


// LOAD CONTROL CLASS - class for automatic system controlling the number of threads 

#define WAITCOUNT 9 // min number of completed task to wait for
#define SCALE 2 // waitcount multiplier
#define DATADEPTH 20 // maximum depth for data arrays

class LoadControl : public QObject
{
	Q_OBJECT

public:
	LoadControl(int ideal_thread_count) : PerfectThreadCount(ideal_thread_count)
	{
		reset(0);
		IsRunning = false;
		ThreadCount = 0;
		UpLockTimer = new QTimer(this);
		connect(UpLockTimer, &QTimer::timeout, this, &LoadControl::unlockUp);
		UpLockTimer->setSingleShot(true);
	}

	void start(int count) // starts automatic managing executing threads
	{
		IsRunning = true;
		ThreadCount = count;
		reset(-count);
		for (int i = 0; i < DATADEPTH; i++)
		{
			TaskTimeArray[i] = QPointF(0, 0);
		}
		UpLock = false;
		UpLockCount = 0;
		SystemState = 0;
	};

	void stop() // stops automatic managing executing threads
	{
		IsRunning = false;
	};

	void reset(int count)
	{
		AvgTime = 0;
		TaskCount = count;
	}

	void setXTimeData(int i, qreal ms)
	{
		if ((TaskTimeArray[i - 1].x() == 0) || (TaskTimeArray[i - 1].x() > ms)) // initialization of x value or ms < current x value
		{
			if ((TaskTimeArray[i - 1].x() == 0) && (TaskTimeArray[i].x() != 0) && (TaskTimeArray[i].x() < ms)) // defence from time scales come till load
			{
				TaskTimeArray[i - 1].setX(TaskTimeArray[i].x());
			}
			else
			{
				TaskTimeArray[i - 1].setX(ms);
				if (TaskTimeArray[i - 1].y() == 0) setYTimeData(i, ms * 2); // initialization of y value 
				emit timeDataChanged(i, QPointF(ms, 0)); // report of x value changes to star scale chart
				qDebug() << "loadcontrol: value set | set X " << ms << " for " << i << "(min)";
			}
		}
		else if (ms / TaskTimeArray[i - 1].x() - 1 > 1.0 / (WAITCOUNT * SCALE)) // ms >> current x value
		{
			qreal new_x = static_cast<qreal>((WAITCOUNT * SCALE * TaskTimeArray[i - 1].x() + ms) / (WAITCOUNT * SCALE + 1));
			TaskTimeArray[i - 1].setX(new_x);
			qDebug() << "loadcontrol: value set | set X " << new_x << " for " << i << "(pull up)";
		}
		else // ms ~ current x value
		{
			qDebug() << "loadcontrol: no X value correction for" << i;
		}
	}

	void setYTimeData(int i, qreal ms)
	{
		if (i >= 1 && ms > TaskTimeArray[i-1].x())
		{
			TaskTimeArray[i-1].setY(ms);
			emit timeDataChanged(i, QPointF(0, ms)); // report of y value changes to star scale chart
			qDebug() << "loadcontrol: value set | set Y " << ms << " for " << i;
		}
	}

	qreal getTimeData(int i)
	{
		return TaskTimeArray[i-1].x();
	}

	void lockUp()
	{
		UpLock = true;
		if (UpLockCount < 10) UpLockCount++;
		if (UpLockCount > 0) UpLockTimer->start(UpLockCount * 1000);
		qDebug() << "loadcontrol: lock up for" << UpLockCount << "sec";
	}

	void updateSystemState(int transition)
	{
		// system state update (default: +1 - add thread; -1 - remove thread)
		SystemState = SystemState << 1;
		if (transition > 0) { SystemState++; }

		// lock up checking (one + two transitions cycles)
		if (SystemState % 16 == 10 || SystemState % 256 == 153 || SystemState % 256 == 204) // last 4 transitions == 1010 or last 8 transitions == 10011001 || 11001100
		{
			lockUp();
		}
		else if (SystemState % 4 == 3 || SystemState % 4 == 0) // two same transition in row (11 or 00)
		{
			UpLockCount = 0;
		}
		else
		{
			// do nothing
		}
	}

public slots:
	void finishedTask(int ms) // processing signal "finished" of any task
	{
		if (IsRunning)
		{
			
			if (changeThread(ms))
			{
				reset(-ThreadCount);
			}

			// counting statistics
			if (TaskCount < 0)
			{
				// waiting state
				AvgTime = 0;
			}
			else if ((TaskCount == 0) && (AvgTime == 0))
				{
					// first counted task
					AvgTime = ms;
				}
				else if (TaskCount > 0)
				{
					// making average statistics
					AvgTime = ((AvgTime * TaskCount) + (qreal)ms) / (TaskCount + 1);
					if (TaskCount > WAITCOUNT*SCALE) { setXTimeData(ThreadCount, AvgTime); reset(-1); }
				}
			TaskCount++;
		}
	};

	bool overloadThread(int ms) // returns true if overload
	{
		if ((qreal)ms > (TaskTimeArray[ThreadCount - 1].y()))
			return true;
		else return false;
	}

	bool underloadThread() // returns true if underload
	{
		if (((TaskTimeArray[ThreadCount].x() != 0) && (TaskTimeArray[ThreadCount - 1].x() != 0)) || (TaskCount >= WAITCOUNT))
			return true;
		else return false;
	}

	bool changeThread(int ms) // returns true if there was a change in running thread number 
	{
		if (TaskTimeArray[ThreadCount - 1].x() == 0) // wait condition (no information about this state)
		{
			return false;
		}
		else if (overloadThread(ms))
		{	
			if (ThreadCount > 1)
			{
				//setXTimeData(ThreadCount, AvgTime);
				emit removeThread(ms);
				return true;
			}
			else
			{
				return false; // wait condition (lowest thread number)
			}
		}
		else if (underloadThread())
		{
			if (ThreadCount < PerfectThreadCount + OVERLOAD)
			{
				if (UpLock)
				{
					return false; // up-state transition is locked
					qDebug() << "load system: unable to complete action | up-state transition is locked";
				}
				else
				{
					//setXTimeData(ThreadCount, AvgTime);
					emit addThread();
					return true;
				}
			}
			else
			{
				return false; // wait condition (highest thread number)
			}
		}
		else
		{
			return false; // wait condition (counting statistics)
		}
	};

	void setThreadCount(int count)
	{
		ThreadCount = count;
	}

	void setTimeData(int i, QPointF ms)
	{
		if (ms.x() != 0) setXTimeData(i, ms.x());
		if (ms.y() != 0) setYTimeData(i, ms.y());
	}

	void unlockUp()
	{ 
		UpLock = false;
		qDebug() << "loadcontrol: unlock up";
	}

private:
	int TaskCount = 0; // number of completed tasks in the relax state
	int WaitCount = 0; // number of completed tasks to wait to change thread number
	int ThreadCount = 0; // current thread number
	qreal AvgTime = 0; // average task completion time at the same thread number
	bool IsRunning = false; // current managing system state
	const int PerfectThreadCount; // const IdealThreadCount
	QPointF TaskTimeArray[DATADEPTH]; // average data for each thread number, x means the lowest and y the biggest possible time scales

	bool UpLock = false; // locks the up-state transition (underload condition)
	QTimer* UpLockTimer; // measures UpLock interval
	quint16 SystemState = 0; // transition set bit flag (last 16 transitions)
	uint UpLockCount = 0; // number of locks in current state

signals:
	void addThread();
	void removeThread(qreal ms);
	void timeDataChanged(int count, QPointF ms);

};


// TASK MANAGER CLASS - class for instant thread pool managing 

class TaskManager : public QObject
{
	Q_OBJECT

public:
	TaskManager(int count) 
	{ 
		setMaxThreadNumber(1);
		PerfectThreadCount = count;
	}
	inline void startThreads(int ThreadNumber); // starts tasks executing by ThreadNumber similar threads
	inline void addThread(); // adds one more thread to do executing tasks
	inline void removeThread(); // removes one thread from running thread pool
	void setCurrentThreadNumber(int num) { CurrentThreadNumber = num; } // sets up the number of running threads
	inline void setMaxThreadNumber(int num);
	void stopThreads() { setMaxThreadNumber(0); MyThreadPool.clear(); } // stops all running threads and deletes all tasks

public slots:
	void addTask(); // creates new ThreadTask to execute and adds it to running thread pool
	void finishTask(ThreadState thread_state);

private:
	int CurrentThreadNumber = 1;
	int PerfectThreadCount; // const IdealThreadCount
	qint64 TaskCount = 0;
	QThreadPool MyThreadPool;

signals:
	void finishTime(int ms);
	void finishThread(ThreadState thread_state);
};


// LOAD CHARTVIEW CLASS - class for load/performance chart data and visualization settings

class LoadChartView : public QChartView
{
	Q_OBJECT

public:
	LoadChartView(int PerfectThreadCount, QWidget* parent = 0)
	{
		//set parent widget
		if (parent != 0) setParent(parent);

		//set average count
		AverageCount = PerfectThreadCount;

		//axis and data settings
		TimeAxis = new QValueAxis;
		TimeAxis->setRange(0.0, TimeVisibleRange);
		TimeAxis->setTickCount(5);
		TimeAxis->setMinorTickCount(4);
		TimeAxis->setLabelFormat("%i");
		TimeAxis->setTitleText("Time, sec");

		LoadAxis = new QValueAxis;
		LoadAxis->setRange(0, PerfectThreadCount + OVERLOAD + 2);
		LoadAxis->setTickCount((PerfectThreadCount + OVERLOAD + 2) / 2 + 1);
		LoadAxis->setMinorTickCount(1);
		LoadAxis->setLabelFormat("%i");
		LoadAxis->setTitleText("Threads, num");

		PerformanceAxis = new QValueAxis;
		PerformanceAxis->setRange(0, 100 * (PerfectThreadCount + OVERLOAD + 2) / (PerfectThreadCount + OVERLOAD) + 1);
		PerformanceAxis->setTickCount((PerfectThreadCount + OVERLOAD + 2) / 2 + 1);
		PerformanceAxis->setMinorTickCount(1);
		PerformanceAxis->setLabelFormat("%i");
		PerformanceAxis->setTitleText("Performance, %");

		LoadSeries = new QLineSeries;
		LoadSeries->setName("Threads");

		PerformanceSeries = new QLineSeries;
		PerformanceSeries->setName("Performance");

		//chart settings
		QChart *chart = new QChart();

		chart->addAxis(this->TimeAxis, Qt::AlignBottom);
		chart->addAxis(this->LoadAxis, Qt::AlignRight);
		chart->addAxis(this->PerformanceAxis, Qt::AlignLeft);

		chart->addSeries(this->LoadSeries);
		LoadSeries->attachAxis(TimeAxis);
		LoadSeries->attachAxis(LoadAxis);

		chart->addSeries(this->PerformanceSeries);
		PerformanceSeries->attachAxis(TimeAxis);
		PerformanceSeries->attachAxis(PerformanceAxis);

		chart->legend()->setAlignment(Qt::AlignTop);
		chart->legend()->setMarkerShape(QLegend::MarkerShapeFromSeries);
		chart->legend()->setShowToolTips(true);

		chart->setMargins(QMargins(10,0,10,10));

		chart->setTheme(QChart::ChartThemeDark);

		//performance array settings
		for (int i = 0; i < DATADEPTH; i++) { PerformanceArray[i] = 0; };

		//timer setting
		ChartUpdateTimer = new QTimer(this);
		connect(ChartUpdateTimer, SIGNAL(timeout()), this, SLOT(updateChart()));
		ChartUpdateTimer->start(100); // 0.1 sec - chart update period

		TimeLine.start(); // so that it always contains current time for axis value

		//chart widget settings
		setChart(chart);
		setRenderHint(QPainter::Antialiasing);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
		update();

		//help menu settings
		HelpMenu = new QMenu(this);
		HelpMenu->addAction("Zoom In", this, &LoadChartView::zoomChartIn, Qt::Key_Up);
		HelpMenu->addAction("Zoom Out", this, &LoadChartView::zoomChartOut, Qt::Key_Down);
		HelpMenu->addAction("Scroll Ahead", this, &LoadChartView::scrollChartAhead, Qt::Key_Right);
		HelpMenu->addAction("Scroll Back", this, &LoadChartView::scrollChartBack, Qt::Key_Left);
		HelpMenu->addSeparator();
		HelpMenu->addAction("Save Chart", this, &LoadChartView::saveChart, Qt::CTRL + Qt::Key_S);
		setStyleSheet("QMenu::separator { height: 1px; background: rgb(100, 100, 100); margin-left: 5px; margin-right: 5px; }");
	};
	int getLastLoadPoint() // returns load value of last added load series point
	{
		return LastLoadPoint;
	}
	void integerPerformancePoint(qreal performance) // puts a new load point at the end of the series integrating with last AVERAGING_DEPTH points
	{
		qreal average = performance;
		for (int i = 0; i < AverageCount; i++) { average += PerformanceArray[i]; } // averaging with last AVERAGING_DEPTH points
		average /= AverageCount + 1;
		addPerformancePoint(average);
		for (int i = 0; i < AverageCount - 1; i++) { PerformanceArray[i] = PerformanceArray[i + 1]; } // performance array shift
		PerformanceArray[AverageCount - 1] = performance;
	}
	void averagePerformancePoint(qreal performance) // puts every AverageCount-th point at the end of the series averaging with last AverageCount points
	{
		qreal average = performance;
		if (PerformanceArray[AverageCount - 1] > 0) // when the perormance array is full -> add the average into series & zeroing performance array
		{
			for (int i = 0; i < AverageCount; i++) { average += PerformanceArray[i]; PerformanceArray[i] = 0; }
			average /= AverageCount + 1;
			addPerformancePoint(average);
		}
		else // add to a first null element new performance value
		{
			for (int i = 0; i < AverageCount; i++) { if (PerformanceArray[i] == 0) { PerformanceArray[i] = average; break; } }
		}
	}
	void zoomChartIn()
	{
		qreal time = TimeLine.elapsed() / 1000;
		if (time < TimeAxis->max() - TimeVisibleRange / 2) 
		{ 
			TimeVisibleRange /= 2;
			TimeAxis->setRange(time - TimeVisibleRange / 2, time + TimeVisibleRange / 2); 
		}
		else 
		{ 
			qreal shift = TimeVisibleRange / 2 / 2;
			TimeVisibleRange /= 2;
			TimeAxis->setRange(TimeAxis->min() + shift, TimeAxis->max() - shift);
		}
		chart()->update();
	}
	void zoomChartOut()
	{
		qreal shift = TimeVisibleRange / 2 * 2;
		TimeVisibleRange *= 2;
		if (TimeAxis->min() < shift) { TimeAxis->setRange(0.0, TimeVisibleRange); }
								else { TimeAxis->setRange(TimeAxis->min() - shift, TimeAxis->max() + shift); }
		chart()->update();
	}
	
private:
	QMenu* HelpMenu;
	QValueAxis* LoadAxis;
	QValueAxis* TimeAxis;
	QValueAxis* PerformanceAxis;
	QLineSeries* LoadSeries;
	QLineSeries* PerformanceSeries;
	QTimer* ChartUpdateTimer;
	QTime TimeLine;
	int AverageCount = 1;

	int LastLoadPoint = 0;
	qreal PerformanceArray[DATADEPTH];
	qreal TimeVisibleRange = 50.0;

protected:
	void wheelEvent(QWheelEvent* event)
	{
		QPoint dangle = event->angleDelta()/8;
		QPoint dpixel = event->pixelDelta();
		if (!dangle.isNull())
			scrollTimeAxis((qreal)(dangle.y())/180*TimeVisibleRange);
		else if (!dpixel.isNull())
			scrollTimeAxis((qreal)(dpixel.x())/180*TimeVisibleRange);

		event->accept();
	}
	void keyPressEvent(QKeyEvent* event)
	{
		switch (event->key()) {
		case Qt::Key_Up:
			zoomChartIn();
			break;
		case Qt::Key_Down:
			zoomChartOut();
			break;
		case Qt::Key_Right:
			scrollChartAhead();
			break;
		case Qt::Key_Left:
			scrollChartBack();
			break;
		case Qt::Key_S:
		{
			if (event->modifiers() == Qt::ControlModifier) { saveChart(); }
			break;
		}
		default:
			break;
		}
		event->accept();
	}
	void mouseReleaseEvent(QMouseEvent* event)
	{
		if (event->button() == Qt::RightButton)
		{
			QPoint event_point = event->globalPos();
			HelpMenu->popup(event_point);
		}
	}

public slots:
	void scrollChartAhead()
	{
		scrollTimeAxis(TimeVisibleRange * 2 / 25);
	}
	void scrollChartBack()
	{
		scrollTimeAxis(TimeVisibleRange * (-2) / 25);
	}
	void addLoadPoint(int load) // puts instantly a new load point at the end of the series (current time axis value added automatically)
								// + time axis transformation (compression/scroll)
	{ 
		qreal time = TimeLine.elapsed() / 1000;
		if ((time >= TimeAxis->max() - 0.05*TimeVisibleRange) && (time <= TimeAxis->max())) { scrollTimeAxis(0.2*TimeVisibleRange); }
		LastLoadPoint = load;
		LoadSeries->append(time, load); 
	} 
	void addPerformancePoint(qreal performance) // puts instantly new performance point at the end of the series (current time axis value added automatically)
	{ 
		qreal time = TimeLine.elapsed() / 1000;
		PerformanceSeries->append(time, performance);
	}
	void scrollTimeAxis(qreal dtime)
	{
		if (TimeAxis->min() + dtime >= 0)
			TimeAxis->setRange(TimeAxis->min() + dtime, TimeAxis->max() + dtime);
		else
			TimeAxis->setRange(0.0, TimeVisibleRange);
		update();
	}
	void updateChart()
	{
		this->update();
		addLoadPoint(LastLoadPoint);
	}
	void saveChart()
	{
		QRect screen = QRect(this->mapToGlobal(QPoint(0, 0)), this->mapToGlobal(rect().bottomRight()));
		QImage image = qApp->screens().at(0)->grabWindow(QDesktopWidget().winId(), screen.left(), screen.top(), screen.width(), screen.height()).toImage();
		image.save("LoadChart.png", "PNG");
		qDebug() << "loadchartview: chart saved | LoadChart.png";
	}
};


// STAR CHARTVIEW CLASS - class for load system time data and visualization settings

class StarChartView : public QChartView
{
	Q_OBJECT

public:
	StarChartView(int PerfectThreadCount, QWidget* parent = 0) : AngleTickNumber(PerfectThreadCount)
	{
		//set parent widget
		if (parent != 0) setParent(parent);

		//chart setting
		QPolarChart *chart = new QPolarChart();
		chart->legend()->hide();
		chart->setMargins(QMargins(5, 0, 5, 5));
		chart->setTheme(QChart::ChartThemeDark);

		RadialAxis = new QValueAxis;
		RadialAxis->setRange(0.0, 1.0);
		RadialAxis->setTickCount(6);
		RadialAxis->setMinorTickCount(1);
		RadialAxis->setLabelFormat("%i");
		RadialAxis->setTitleText("Time, msec");

		AngleAxis = new QValueAxis;
		AngleAxis->setRange(1, AngleTickNumber + 1);
		AngleAxis->setTickCount(AngleTickNumber + 1);
		AngleAxis->setLabelFormat("%i");
		AngleAxis->setTitleText("Threads");

		LowerScaleSeries = new QLineSeries;
		UpperScaleSeries = new QLineSeries;
		for (int i = 0; i < AngleTickNumber + 1; i++) { LowerScaleSeries->append(i + 1, 0.0); UpperScaleSeries->append(i + 1, 0.0); }

		ScaleSeries = new QAreaSeries(UpperScaleSeries, LowerScaleSeries);
		ScaleSeries->setPointLabelsVisible(true);
		ScaleSeries->setPointLabelsClipping(false);
		ScaleSeries->setPointLabelsFormat("@yPoint");
		ScaleSeries->setName("Time Scale");

		OverloadSeries = new QScatterSeries();
		OverloadSeries->setPointLabelsVisible(false);
		OverloadSeries->setPointLabelsClipping(false);
		OverloadSeries->setPointLabelsFormat("@yPoint");
		OverloadSeries->setName("Overload");
		OverloadSeries->setMarkerShape(QScatterSeries::MarkerShapeCircle);
		OverloadSeries->setMarkerSize(7.5);

		connect(ScaleSeries, &QAreaSeries::pressed, this, &StarChartView::pressedPoint);
		//connect(ScaleSeries, &QAreaSeries::released, this, &StarChartView::releasedPoint);
		connect(OverloadSeries, &QScatterSeries::pressed, this, &StarChartView::pressedPoint);
		//connect(OverloadSeries, &QScatterSeries::released, this, &StarChartView::releasedPoint);

		chart->addAxis(RadialAxis, QPolarChart::PolarOrientationRadial);
		chart->addAxis(AngleAxis, QPolarChart::PolarOrientationAngular);

		chart->addSeries(this->ScaleSeries);
		ScaleSeries->attachAxis(RadialAxis);
		ScaleSeries->attachAxis(AngleAxis);

		chart->addSeries(this->OverloadSeries);
		OverloadSeries->attachAxis(RadialAxis);
		OverloadSeries->attachAxis(AngleAxis);

		//chart widget setting
		setChart(chart);
		setRenderHint(QPainter::Antialiasing);
		update();

		ScaleSeries->setBorderColor(ScaleSeries->color());

		//help menu settings
		HelpMenu = new QMenu(this);
		HelpMenu->addAction("Zoom In", this, &StarChartView::zoomChartIn, Qt::Key_Up);
		HelpMenu->addAction("Zoom Out", this, &StarChartView::zoomChartOut, Qt::Key_Down);
		HelpMenu->addAction("Clear Series", this, &StarChartView::clearOverloadSeries, Qt::CTRL + Qt::Key_X);
		HelpMenu->addSeparator();
		HelpMenu->addAction("Save Chart", this, &StarChartView::saveChart, Qt::CTRL + Qt::Key_S);
		setStyleSheet("QMenu::separator { height: 1px; background: rgb(100, 100, 100); margin-left: 5px; margin-right: 5px; }");
	};
	bool resizeRadialRange(qreal point, qreal scale = 1.5)
	{
		if (point * scale > RadialAxis->max()) // need to resize radial axis
		{
			if (RadialScaleNumber == 0) // radial axis isn't not calibrated yet
			{
				calibrateRadialAxis(point);
			}

			if (RadialScaleNumber > 0) // scales > 1
			{
				RadialAxis->setMax(round((point * scale / pow(10, RadialScaleNumber - 1)) + 0.5) * pow(10, RadialScaleNumber - 1));
			}
			else if (RadialScaleNumber < 0) // scales < 1
			{
				RadialAxis->setMax(round((point * scale / pow(10, RadialScaleNumber)) + 0.5) * pow(10, RadialScaleNumber));
			}
			else 
			{
				qDebug() << "starchartview: unexpected value | radial axis isn't calibrated";
			}
			chart()->update();
			return true;
		}
		else
		{
			return false;
		}

	}
	bool calibrateRadialAxis(qreal point)
	{
		int base = 0;
		if (point >= 1)
		{
			for (base = 1; (point / pow(10, base)) >= 1; base++);
			RadialScaleNumber = base;
			return true;
		}
		else if (point >= 0)
		{
			for (base = -1; (point / pow(10, base)) < 1; base--);
			RadialScaleNumber = base;
			return true;
		}
		else
		{
			qDebug() << "starchartview: incorrect value | value is below zero";
			return false;
		}
	}
	void clearOverloadSeries()
	{
		OverloadSeries->clear();
	}
	void zoomChartIn()
	{

	}
	void zoomChartOut()
	{

	}

private:
	QMenu* HelpMenu;
	QLineSeries* LowerScaleSeries;
	QLineSeries* UpperScaleSeries;
	QAreaSeries* ScaleSeries;
	QScatterSeries* OverloadSeries;
	QValueAxis* RadialAxis;
	QValueAxis* AngleAxis;
	QPoint ScreenPoint = QPoint(0,0);
	QPointF ChartPoint = QPointF(0.0, 0.0);
	int RadialScaleNumber = 0;
	const int AngleTickNumber;

protected:
	void keyPressEvent(QKeyEvent* event)
	{
		switch (event->key()) {
		case Qt::Key_Up:
			zoomChartIn();
			break;
		case Qt::Key_Down:
			zoomChartOut();
			break;
		case Qt::Key_S:
		{
			if (event->modifiers() == Qt::ControlModifier) { saveChart(); }
			break;
		}
		case Qt::Key_X:
		{
			if (event->modifiers() == Qt::ControlModifier) { clearOverloadSeries(); }
			break;
		}
		default:
			break;
		}
		event->accept();
	}
	void mouseReleaseEvent(QMouseEvent* event)
	{
		if (event->button() == Qt::RightButton)
		{
			QPoint event_point = event->globalPos();
			HelpMenu->popup(event_point);
		}
		else if (event->button() == Qt::LeftButton)
		{
			if (ChartPoint.x() != 0 && ChartPoint.y() != 0)
				releasedPoint();
		}
		event->accept();
	}

public slots:
	void updateChart()
	{
		chart()->update();
	}
	void saveChart()
	{
		QRect screen = QRect(this->mapToGlobal(QPoint(0, 0)), this->mapToGlobal(rect().bottomRight()));
		QImage image = qApp->screens().at(0)->grabWindow(QDesktopWidget().winId(), screen.left(), screen.top(), screen.width(), screen.height()).toImage();
		image.save("StarScaleChart.png", "PNG");
		qDebug() << "starchartview: chart saved | StarScaleChart.png";
	}
	void addScalePoint(int tick, QPointF point)
	{
		if (tick <= AngleTickNumber && tick >= 1)
		{ 
			if (point.x() != 0) LowerScaleSeries->replace(tick - 1, QPointF(tick, point.x())); 
			if (point.y() != 0) UpperScaleSeries->replace(tick - 1, QPointF(tick, point.y()));
			resizeRadialRange(point.y());
		}
	}
	void addOverloadPoint(int tick, qreal point)
	{
		if (tick <= AngleTickNumber && tick >= 1)
		{
			OverloadSeries->append(QPointF(tick, point));
			resizeRadialRange(point);
		}
	}
	void pressedPoint(QPointF point)
	{
		ScreenPoint.setX(QWidget::mapFromGlobal(QCursor::pos()).x());
		ScreenPoint.setY(QWidget::mapFromGlobal(QCursor::pos()).y());
		ChartPoint = point;
		//qDebug() << "starscalechart: pressed point | x =" << point.x() << "y =" << point.y();
	}
	void releasedPoint()             
	{
		QPointF point = ChartPoint;
		//qDebug() << "starscalechart: released point | x =" << point.x() << "y =" << point.y();
		// clear chart point
		ChartPoint.setX(0.0);
		ChartPoint.setY(0.0);
		// need to deal with non-integer angle axis coordinates (coming from line series)
		qreal offset = point.x() - (int)point.x();
		if (offset > 0.1 && offset < 0.9) // x-coordinate is too far from the points in series so we consider this a mistake
			return;

		QPoint NewScreenPoint = QPoint(QWidget::mapFromGlobal(QCursor::pos()).x(), QWidget::mapFromGlobal(QCursor::pos()).y());
		QPoint CenterScreenPoint = QPoint(size().width()/2, size().height()/2); // set center point in widget coordinates
		
		QPointF s1 = ScreenPoint - CenterScreenPoint;
		QPointF s2 = NewScreenPoint - ScreenPoint;
		
		// (s2,s1)/|(s1,s1)| - projection of s2 on s1 in dimension of s1
		qreal projection = ( s2.x() * s1.x() + s2.y() * s1.y() ) / ( s1.x() * s1.x() + s1.y() * s1.y() ); 

		if (offset == 0.0) // x-coordinate of the point is integer
		{
			// this might only be a point from scatter series (OverloadSeries)
			QPointF _point = QPointF(point.x(), (1 + projection)*point.y());
			// if the new point' y-coordinate is below zero (shouldn't be) set it to zero
			if (_point.y() < 0) _point.setY(0); 
			// replace the point with the new
			if (_point.y() > UpperScaleSeries->at((int)_point.x() - 1).y() || _point.y() < LowerScaleSeries->at((int)_point.x() - 1).y()) // the new point position is in allowed area -> replace
			{
				OverloadSeries->replace(point, _point);
				resizeRadialRange(point.y());
			}
			else 
			{
				// --- is in restricted area -> remove (now -> do nothing << restricted memory access error)
			}
		}
		else // x-coordinate of the point is non-integer
		{
			// this might only be a point from line series (UpperScaleSeries)
			// firstly we should find the closest point from series and then replace it with the new
			int roundX = round(point.x());
			QPointF _point = QPointF(roundX, (1 + projection) * UpperScaleSeries->at(roundX-1).y());
			emit changedScalePoint(_point.x(), QPointF(0, _point.y())); // tell the load system to change the scale point
		}
	}

signals:
	void changedScalePoint(int tick, QPointF point);

};


// BAR CHARTVIEW CLASS - class for thread task data and visualization settings

class BarChartView : public QChartView
{
	Q_OBJECT

public:
	BarChartView(QWidget* parent = 0, uint ideal_thread_count = 12, uint precision = 2, uint depth = 10)
		: PerfectThreadCount(ideal_thread_count), PerformancePrecision(precision), ThreadStateDepth(depth)
	{
		//set parent widget
		if (parent != 0) setParent(parent);

		//thread base settings
		ThreadGlobalBase.clear();
		ThreadLocalBase.clear();

		//chart settings
		QChart* chart = new QChart();
		chart->setTheme(QChart::ChartThemeDark);

		TaskAxis = new QValueAxis;
		TaskAxis->setRange(0, 1);
		TaskAxis->setTickCount(3);
		TaskAxis->setMinorTickCount(1);
		TaskAxis->setLabelFormat("%." + QString::number(PerformancePrecision-1) + "f");
		TaskAxis->setTitleText("Performance, tasks/sec");

		ThreadAxis = new QBarCategoryAxis;
		//ThreadAxis->setTitleText("Thread");
		ThreadAxis->setLabelsAngle(-90);

		ThreadGlobalSet = new QBarSet("Global");
		ThreadLocalSet = new QBarSet("Local");

		ThreadSeries = new QBarSeries();
		ThreadSeries->append(ThreadGlobalSet);
		ThreadSeries->append(ThreadLocalSet);
		ThreadSeries->setLabelsVisible(true);
		ThreadSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
		ThreadSeries->setLabelsAngle(-45);

		chart->addAxis(ThreadAxis, Qt::AlignBottom);
		chart->addAxis(TaskAxis, Qt::AlignLeft);

		chart->addSeries(ThreadSeries);
		ThreadSeries->attachAxis(ThreadAxis);
		ThreadSeries->attachAxis(TaskAxis);

		//chart->legend()->hide();
		chart->setMargins(QMargins(15, 5, 15, 15));
		chart->setAnimationOptions(QChart::NoAnimation);

		setChart(chart);
		setRenderHint(QPainter::Antialiasing);
		update();

		QColor Bgreen = ThreadGlobalSet->color();
		QColor Bblue = ThreadLocalSet->color();
		Bgreen.setGreenF(0.9);
		Bblue.setBlueF(1.0);
		ThreadGlobalSet->setLabelColor(Bgreen);
		ThreadGlobalSet->setBorderColor(ThreadGlobalSet->color());
		ThreadLocalSet->setLabelColor(Bblue);
		ThreadLocalSet->setBorderColor(ThreadLocalSet->color());

		//timer update settings
		ChartUpdateTimer = new QTimer(this);
		connect(ChartUpdateTimer, SIGNAL(timeout()), this, SLOT(addChartPerformance()));
		ChartUpdateTimer->start(300); // 0.3 sec - chart update period

		//help menu settings
		HelpMenu = new QMenu(this);
		HelpMenu->addAction("Label Format", this, &BarChartView::changeLabelFormat, Qt::CTRL + Qt::Key_F);
		HelpMenu->addSeparator();
		HelpMenu->addAction("Save Chart", this, &BarChartView::saveChart, Qt::CTRL + Qt::Key_S);
		setStyleSheet("QMenu::separator { height: 1px; background: rgb(100, 100, 100); margin-left: 5px; margin-right: 5px; }");
	};
	inline int checkThread(ThreadState thread_state); // checks the existence of certain thread in ThreadBase, returns its thread_id or -1 if no instance is found
	inline void clearChart(); // clears all chart data
	inline void clearBase(); // clears all base data

public slots:
	inline void addChartPerformance();
	void changeLabelFormat() 
	{ 
		ThreadAxisLabelFormat = !ThreadAxisLabelFormat; 
		qDebug() << "barchartview: change label format | full -" << ThreadAxisLabelFormat; 
	}
	inline void saveChart();
	void addFinishedTask(const ThreadState thread_state)
	{
		int thread_id = checkThread(thread_state);
		if (thread_id == -1) // not found
		{
			addNewThread(thread_state);
		}
		else // found
		{
			addThreadState(thread_id, thread_state);
		}
	}
    void killThreadSlot()
    {
        QThread* thread_pointer = static_cast<QThread*>(sender());
        uint ThreadBaseLength = ThreadGlobalBase.length();
        int thread_id = -1;
        for (uint i = 0; i < ThreadBaseLength; i++)
        {
            if (thread_pointer == ThreadGlobalBase[i].getPointer())
            {
                thread_id = i;
            }
        }
		if (!ThreadGlobalBase[thread_id].isKilled())
		{
			if (thread_id >= 0 && thread_id < ThreadBaseLength)
			{
				ThreadGlobalBase[thread_id].kill();
				qDebug() << "barchartview: thread" << ThreadGlobalBase[thread_id].getName() << "is killed from" << thread_id << "by kill thread slot";
			}
			else
			{
				qDebug() << "barchartview: incorrect value | thread_id is out of range :" << thread_id;
			}
		}
		else
		{
			qDebug() << "barchartview: unable to complete action | threadstate is already killed";
		}
    }

protected:
	void keyPressEvent(QKeyEvent* event)
	{
		switch (event->key()) {
		case Qt::Key_S:
		{
			if (event->modifiers() == Qt::ControlModifier) { saveChart(); }
			break;
		}
		case Qt::Key_F:
		{
			if (event->modifiers() == Qt::ControlModifier) { changeLabelFormat(); }
			break;
		}
		default:
			break;
		}
		event->accept();
	}
	void mouseReleaseEvent(QMouseEvent* event)
	{
		if (event->button() == Qt::RightButton)
		{
			QPoint event_point = event->globalPos();
			HelpMenu->popup(event_point);
		}
	}

private:
	QMenu* HelpMenu;
	QBarSet* ThreadGlobalSet;
	QBarSeries* ThreadSeries;
	QBarSet* ThreadLocalSet;
	QValueAxis* TaskAxis;
	QBarCategoryAxis* ThreadAxis;
	QList<ThreadState> ThreadGlobalBase;
	QList<ThreadState> ThreadLocalBase;
	QTimer* ChartUpdateTimer;
	const uint PerformancePrecision;
	const uint ThreadStateDepth;
	const uint PerfectThreadCount;
	bool ThreadAxisLabelFormat = false; // false = reduced, true = full
	inline void addThreadState(uint thread_id, ThreadState thread_state); // adds information about ended task in certain thread to ThreadBase
	inline int addNewThread(ThreadState thread_state); // adds information about new thread to ThreadBase
	inline void killThread(uint thread_id);
	inline void resizeTaskAxis(qreal ratio = 1.5);

signals:

};

int BarChartView::checkThread(ThreadState thread_state)

{
	int thread_id = -1;
	if (!ThreadGlobalBase.isEmpty())
	{
		uint ThreadBaseLenght = ThreadGlobalBase.length();
		QString thread = thread_state.getName();
		for (uint i = 0; i < ThreadBaseLenght; i++)
		{
			if (!ThreadGlobalBase[i].isKilled()) // checking if threadstate is alive
			{
				if (ThreadGlobalBase[i].getName() == thread)
					thread_id = i;
			}
		}
	}
	return thread_id;
}

void BarChartView::addChartPerformance()
{
	clearChart();
	uint last = ThreadGlobalBase.length();
	if (last > PerfectThreadCount) // number of active threads is over limit
	{
		for (int i = 0; i < last; i++)
		{
			// polling active threads
			if (!ThreadGlobalBase[i].isKilled() && (ThreadGlobalBase[i].getPointer()->isFinished() || !ThreadGlobalBase[i].getPointer()->isRunning()))
			{
				ThreadGlobalBase[i].kill();
				qDebug() << "barchartview: thread" << ThreadGlobalBase[i].getName() << "is killed from" << i << "by polling active threads";
			}
		}
	}

	for (int i = 0; i < last; i++)
	{
		if (ThreadAxisLabelFormat) { ThreadAxis->append(ThreadGlobalBase[i].getName() + " " + ThreadGlobalBase[i].getPriorityString()); }
		else { ThreadAxis->append(ThreadGlobalBase[i].getName()); }
		*ThreadGlobalSet << ThreadGlobalBase[i].getPerformanceRound(PerformancePrecision);
		if (ThreadGlobalBase[i].isKilled())
		{
			*ThreadLocalSet << 0.0;
		}
		else
		{
			*ThreadLocalSet << ThreadLocalBase[i].getPerformanceRound(PerformancePrecision);
		}
	}
	resizeTaskAxis();
}

void BarChartView::addThreadState(uint thread_id, ThreadState thread_state)
{
	if (ThreadGlobalBase.length() > thread_id && ThreadLocalBase.length() > thread_id)
	{
		ThreadGlobalBase[thread_id] += thread_state;
		ThreadLocalBase[thread_id] += thread_state;
	}
}

int BarChartView::addNewThread(ThreadState thread_state)
{
	if (ThreadGlobalBase.length() == ThreadLocalBase.length())
	{
		uint last = ThreadGlobalBase.length();
		uint label = last;
		for (int i = 0; i < last; i++)
		{
			if (ThreadGlobalBase[i].isKilled() /*&& (ThreadGlobalBase[i].getPriority() == thread_state.getPriority())*/)
			{
				label = i;
				qDebug() << "found killed thread from" << i;
				break;
			}
		}
		if (label == last)
		{
			ThreadGlobalBase.append(thread_state);
			ThreadLocalBase.append(thread_state);
		}
		else
		{
			ThreadGlobalBase.replace(label, thread_state);
			ThreadLocalBase.replace(label, thread_state);
		}
		ThreadLocalBase[label].setLimit(ThreadStateDepth);
		bool done = connect(ThreadGlobalBase[label].getPointer(), &QThread::finished, this, &BarChartView::killThreadSlot);
		if (done)
			qDebug() << "barchartview: thread" << ThreadGlobalBase[label].getName() << "is connected to" << label;
		return label;
	}
	else return -1;
}

void BarChartView::clearChart()
{
	uint length = ThreadGlobalSet->count();
	if (length == ThreadLocalSet->count())
	{
		ThreadGlobalSet->remove(0, length);
		ThreadLocalSet->remove(0, length);
	}
	ThreadAxis->clear();
}

void BarChartView::clearBase()
{
	if (ThreadGlobalBase.count() > 0)
	{
		ThreadGlobalBase.clear();
		ThreadLocalBase.clear();
	}
}

void BarChartView::saveChart()
{
	QRect screen = QRect(this->mapToGlobal(QPoint(0, 0)), this->mapToGlobal(rect().bottomRight()));
	QImage image = qApp->screens().at(0)->grabWindow(QDesktopWidget().winId(), screen.left(), screen.top(), screen.width(), screen.height()).toImage();
	image.save("BarThreadChart.png", "PNG");
	qDebug() << "barchartview: chart saved | BarThreadChart.png";
}

void BarChartView::resizeTaskAxis(qreal ratio)
{
	uint last = ThreadGlobalBase.length();
	for (int i = 0; i < last; i++)
	{
		if (TaskAxis->max() < ratio * ThreadLocalBase[i].getPerformanceRound(PerformancePrecision))
		{
			uint max_tasks = round(ratio * ThreadLocalBase[i].getPerformance());
			TaskAxis->setMax(max_tasks);
			while (max_tasks > 10) { max_tasks /= 2; max_tasks++; }
			TaskAxis->setTickCount(max_tasks + 1);
		}
	}
}


// MINOR WINDOW CLASS - class for separate widget displaying

class MinorWindow : public QMainWindow
{
	Q_OBJECT

public:
	MinorWindow(QWidget* widget, QSize size, QMainWindow *parent = 0)
	{
		setCentralWidget(widget);
		setMouseTracking(true);
		setMinimumSize(size);
		resize(size);
		QIcon icon = QIcon("Resources/context_icon1.png");
		if (!icon.isNull()) { this->setWindowIcon(icon); }
		else { qDebug() << "minorwindow: invalid pointer | couldn't set icon"; }
	}

public slots:
	void changeState(int state = 0)
	{
		if (state == 0) { hide(); }
		else { show(); }
	}

protected:
	void closeEvent(QCloseEvent* event)
	{
		emit closed();
		event->accept();
	}

signals :
	void closed();
};


// WINDOW CONTROL CHECK BOX - class for a check box contolling a MINOR WINDOW

class WindowControlCheckBox : public QCheckBox
{
	Q_OBJECT

public:
	WindowControlCheckBox(MinorWindow* _window, const QString name = "")
	{
		if (_window != 0)
		{
			window = _window;
			setText(name + "\n Display");
			window->setWindowTitle(name);
			setStyleSheet("spacing: 10px; font: bold 8pt Tahoma;");
			setChecked(false);
			connect(this, SIGNAL(stateChanged(int)), window, SLOT(changeState(int)));
			connect(window, SIGNAL(closed()), this, SLOT(windowClosed()));
		}
	}

public slots:
	void windowClosed() { setChecked(false); }

private:
	MinorWindow* window = 0;
};


// PARALLEL SYSTEM CLASS - class for the main app window

class parallelsystem : public QMainWindow
{
	Q_OBJECT

public:
	parallelsystem(QWidget *parent = 0);
	~parallelsystem();

	void init(); // setup GUI settings

public slots:
	void startThreads(); // starts number of threads given by spinbox field 
	void stopThreads(); // stops all running threads 
	void changeState(); // changes program state to 'running' if its currently 'waiting' and back
	void addThread(); // adds one new thread to current running thread pool
	void removeThread(qreal ms); // removes one running thread from thread pool
	void finishTask(int ms); // processing signal 'finished' emitted from task manager
	void addThreadManual() { addThread(); }; // manual adding one thread by user
	void removeThreadManual() { removeThread(0.0); }; // manual removing one thread by user
protected:
	void closeEvent(QCloseEvent* event)
	{
		emit closed();
		event->accept();
	}

private:
	TaskManager* MyTaskManager;
	LoadControl* System;

	QPushButton* StartButton;
	QPushButton* AddButton;
	QPushButton* RemoveButton;
	QSpinBox* ThreadNumberBox;
	WindowControlCheckBox* StarDisplayBox;
	WindowControlCheckBox* BarDisplayBox;
	QTextEdit* InfoEdit;
	LoadChartView* LoadChart;
	StarChartView* StarScaleChart;
	BarChartView* BarThreadChart;

	bool IsRunning; // determines current program state
	int PerfectThreadCount; // constant determined by QThread::IdealThreadCount

signals:
	void closed();
};

#endif // PARALLELSYSTEM_H
