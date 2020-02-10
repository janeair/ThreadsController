#ifndef THREADBASE_H
#define THREADBASE_H

#include <qthread.h>
#include <qdebug.h>

class ThreadState
{

public:
	ThreadState(QString id = "0x0000", QThread* pointer = 0, qint32 time = 0, QThread::Priority priority = QThread::InheritPriority, quint32 limit = pow(2,32) - 1)
			: ThreadName(id), ThreadPointer(pointer),ThreadTime(time), ThreadTasks(1), ThreadPriority(priority), ThreadTasksLimit(limit), IsKilled(false) {}

	qint32& getTime() { return ThreadTime; }
	quint32& getTasks() { return ThreadTasks; }
	qreal getPerformance() { if (ThreadTime == 0) { return 0.0; } else { return (qreal)ThreadTasks * 1000 / ThreadTime; }} // return performance in tasks per second
	inline qreal getPerformanceRound(uint precision); // returns performance in tasks per second with 'precision' decimal places
	QThread::Priority& getPriority() { return ThreadPriority; }
	QString& getName() { return ThreadName; }
	QThread* getPointer() { return ThreadPointer; }
	bool& isKilled() { return IsKilled; }

	inline ThreadState& operator+=(const ThreadState& value);

	inline void addTask(int time);
	//void replaceThread(QString id = "0x0000", QThread::Priority priority = QThread::InheritPriority) { ThreadName = id;  ThreadPriority = priority; }
	void kill() { IsKilled = true; }
	inline void setLimit(uint limit);
	inline QString getPriorityString();
	inline void clear(); // kills threadstate and clears the task/time data

private:
	QString ThreadName;
	QThread* ThreadPointer;
	QThread::Priority ThreadPriority;
	qint32 ThreadTime; // ms
	quint32 ThreadTasks; // count
	quint32 ThreadTasksLimit; // max tasks capasity for this thread
	bool IsKilled; // tells if the thread is killed by threadpool (when the threadstate is killed data modification is no longer available)
};

ThreadState& ThreadState::operator+=(const ThreadState& value) // adds data (time & tasks) with rewriting ThreadName and ThreadPriority if needed
{
	if (!this->IsKilled)
	{
		this->ThreadName = value.ThreadName;
		this->ThreadPriority = value.ThreadPriority;
		this->ThreadTime += value.ThreadTime;
		this->ThreadTasks += value.ThreadTasks;
		if (this->ThreadTasks > this->ThreadTasksLimit) // overload -> lossy compression
		{
			quint32 diff = this->ThreadTasks - this->ThreadTasksLimit;
			qreal avg = (qreal)this->ThreadTime / this->ThreadTasks;
			this->ThreadTasks -= diff;
			this->ThreadTime -= round(avg * diff);
		}
		return *this;
	}
	else
	{
		qDebug() << "threadstate: access denied | threadstate is killed";
		return *this;
	}
}

qreal ThreadState::getPerformanceRound(uint precision)
{
	if (ThreadTime == 0)
	{
		return 0.0;
	}
	else
	{
		if (precision > 0)
			return round((qreal)ThreadTasks * 1000 / ThreadTime * pow(10, precision)) / pow(10, precision);
		else
			return getPerformance();
	}
}

void ThreadState::addTask(int time)
{	
	if (!this->IsKilled)
	{
		if (time > 0)
		{
			if (ThreadTasks < ThreadTasksLimit) // below limit
			{
				ThreadTime += time; ThreadTasks++;
			}
			else // above limit
			{
				ThreadTime = ThreadTime * ((qreal)(ThreadTasks - 1) / ThreadTasks) + time;
			}
		}
		else
		{
			qDebug() << "threadstate: incorrect value | time is below zero";
		}
	}
	else
	{
		qDebug() << "threadstate: access denied | threadstate is killed";
	}
}

void ThreadState::setLimit(quint32 limit)
{
	if (!this->IsKilled)
	{
		ThreadTasksLimit = limit;
		if (ThreadTasks > ThreadTasksLimit) // overload -> lossy compression
		{
			quint32 diff = ThreadTasks - ThreadTasksLimit;
			qreal avg = (qreal)ThreadTime / ThreadTasks;
			ThreadTasks -= diff;
			ThreadTime -= round(avg * diff);
		}
	}
	else
	{
		qDebug() << "threadstate: access denied | threadstate is killed";
	}
}

QString ThreadState::getPriorityString()
{
	QString priority_string;
	switch (ThreadPriority)
	{
	case QThread::LowestPriority: case QThread::LowPriority: { priority_string = "L"; break; }
	case QThread::NormalPriority: { priority_string = "N"; break; }
	case QThread::HighPriority: case QThread::HighestPriority: { priority_string = "H"; break; }
	case QThread::TimeCriticalPriority: { priority_string = "TC"; break; }
	default: { priority_string = "?"; break; }
	}
	return priority_string;
}

void ThreadState::clear()
{
	IsKilled = true;
	ThreadTime = 0;
	ThreadTasks = 0;
}

#endif // THREADBASE_H