#define DEBUG

#define ALL_COMPLETE 2

#include <thread>
#include <chrono>
#include <string>
#include <mutex>
#include <iostream>

using namespace std;

typedef void* (*thread_func_ptr)(void*);

int numCycles = 5;

/*****************************************/
bool clockEdge = false;
mutex clockMutex;

int countClockedComplete = 0;
bool clockedComplete = false;
mutex clockedCompleteMutex;

int countCycleComplete = 0;
bool cycleComplete = false;
mutex cycleMutex;

bool executionComplete = false;
mutex completeMutex;

/*****************************************/
mutex outputLock;

void dbg(string output)
{
    #ifdef DEBUG
        outputLock.lock();
        cout << output << endl;
        flush(cout);
        outputLock.unlock();
    #endif
}

/*****************************************/
bool CheckClockRisingEdge()
{
    bool risingEdge;
    clockMutex.lock();
    risingEdge = clockEdge;
    clockMutex.unlock();
    return risingEdge;
}

/*****************************************/
bool CheckClockFallingEdge()
{
    bool fallingEdge;
    clockMutex.lock();
    fallingEdge = !clockEdge;
    clockMutex.unlock();
    return fallingEdge;
}

/*****************************************/
void SetClockEdge(bool risingEdge)
{
    clockMutex.lock();
    clockEdge = risingEdge;
    clockMutex.unlock();
}

/*****************************************/
void SetClocked()
{
    clockedCompleteMutex.lock();
    countClockedComplete++;
    if(countClockedComplete == ALL_COMPLETE)
    {
        countClockedComplete = 0;
        clockedComplete = true;
    }
    clockedCompleteMutex.unlock();
}

/*****************************************/
bool CheckClocked()
{
    bool clocked;
    clockedCompleteMutex.lock();
    clocked = clockedComplete;
    clockedCompleteMutex.unlock();
    return clocked;
}

/*****************************************/
void SetCycleComplete()
{
    completeMutex.lock();
    countCycleComplete++;
    if(countCycleComplete == ALL_COMPLETE)
    {
        countCycleComplete = 0;
        cycleComplete = true;
    }
    completeMutex.unlock();
}

/*****************************************/
bool CheckCycleComplete()
{
    bool complete;
    completeMutex.lock();
    complete = cycleComplete;
    completeMutex.unlock();
    return complete;
}

/*****************************************/
void SetExecutionComplete()
{
    completeMutex.lock();
    executionComplete = true;
    completeMutex.unlock();
}

/*****************************************/
bool CheckExecutionComplete()
{
    bool complete;
    completeMutex.lock();
    complete = executionComplete;
    completeMutex.unlock();
    return complete;
}

/**************************/
struct InstructionString
{
    string opcode;
    string destReg;
    string srcReg1;
    string srcReg2;

    string ToString()
    {
        return "<" + opcode + ","
                   + destReg + ","
                   + srcReg1 + ","
                   + srcReg2 + ">";
    }
};

/**************************/
struct InstructionDecode
{
    string opcode;
    int destRegNum;
    int srcReg1Num;
    int srcReg2Num;

    string ToString()
    {
        return "<" + opcode + ","
                   + to_string(destRegNum) + ","
                   + to_string(srcReg1Num) + ","
                   + to_string(srcReg2Num) + ">";
    }
};

/**************************/
struct MemoryValue
{
    int location;
    int value;

    string ToString()
    {
        return "<" + to_string(location) + ","
                   + to_string(value) + ">";
    }
};

/**************************/
struct RegisterValue
{
    int regNum;
    int value;

    string ToString()
    {
        return "<R" + to_string(regNum) + ","
                   + to_string(value) + ">";
    }
};

/******************************************************************************************/
class CycleThread
{
public:
	CycleThread(void* (*f)(void*), void* obj)
	{
        this->RunFunction = f;
        object = obj;
        Activate();
	}
	virtual ~CycleThread(){}
	void Activate()
	{
        if(pthread_create(&threadID, NULL, RunFunction, object))
            isActive = false;
        else
            isActive = true;
    }
	void JoinThread(){if(isActive) pthread_join(threadID, NULL);}

private:
    bool isActive;
    void* (*RunFunction)(void*);
    void* object;
    pthread_t threadID;
};

/******************************************************************************************/
template <class In, class Out>
class Buffer
{
public:
    Buffer(thread_func_ptr thrPtr) : thread_func(thrPtr){ isEmpty = true; Initialize(); StartThread();};
    /******************************************/
    bool Empty()
    {
        if(isEmpty)
        {
            readLock.lock();
            hasRead = true;
            readLock.unlock();
        }
        return isEmpty;
    }
    /******************************************/
    void NoWrite()
    {
        writeLock.lock();
        noWrite = true;
        hasWritten = true;
        writeLock.unlock();
    }
    /******************************************/
    void Write(In writeInput)
    {
        writeLock.lock();
        noWrite = false;
        hasWritten = true;
        tempInput = writeInput;
        writeLock.unlock();
    }
    /******************************************/
    Out Read()
    {
        Out outputData;
        readLock.lock();
        outputData = ReadData();
        hasRead = true;
        readLock.unlock();
        return outputData;
    }
    /******************************************/
    void Join()
    {
        thread->JoinThread();
    }
    /******************************************/
    virtual string GetContent() = 0;

protected:
    CycleThread* thread;
    thread_func_ptr thread_func;
    In tempInput;
    bool isEmpty;
    bool noWrite;

    mutex writeLock, readLock;
    bool hasRead, hasWritten;

    bool ReadComplete()
    {
        bool readCmplt;
        readLock.lock();
        readCmplt = hasRead;
        readLock.unlock();
        return readCmplt;
    }

    bool WriteComplete()
    {
        bool writeCmplt;
        writeLock.lock();
        writeCmplt = hasWritten;
        writeLock.unlock();
        return writeCmplt;
    }

    virtual void Initialize(){};
    virtual void StartThread(){thread = new CycleThread(thread_func, (void*) this);};
    virtual Out ReadData() = 0;
    virtual Out TestRead() = 0;
    virtual void WriteData() = 0;
};

/******************************************************************************************/
void* thread_LIB(void* object);
class LoadInstrBuffer : public Buffer<InstructionDecode, InstructionDecode>
{
public:
    friend void* thread_LIB(void* object);
    LoadInstrBuffer() : Buffer(thread_LIB){}

    string GetContent()
    {
        if(!isEmpty)
            return "LIB:" + currentInstr.ToString();
        else
            return "LIB:";
    }

private:
    InstructionDecode currentInstr;

    /******************************************/
    InstructionDecode ReadData() {return currentInstr;}

    /******************************************/
    void WriteData() {currentInstr = tempInput;}

    /******************************************/
    InstructionDecode TestRead() {return ReadData();}
};

void* thread_LIB(void* object)
{
    LoadInstrBuffer* LIB = (LoadInstrBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetClocked();

dbg(LIB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_AIB(void* object);
class ArithInstrBuffer : public Buffer<InstructionDecode, InstructionDecode>
{
public:
    friend void* thread_AIB(void* object);
    ArithInstrBuffer() : Buffer(thread_AIB){}

    string GetContent()
    {
        if(!isEmpty)
            return "AIB:" + currentInstr.ToString();
        else
            return "AIB:";
    }

private:
    InstructionDecode currentInstr;

    /******************************************/
    InstructionDecode ReadData() {return currentInstr;}

    /******************************************/
    void WriteData() {currentInstr = tempInput;}

    /******************************************/
    InstructionDecode TestRead() {return ReadData();}
};

void* thread_AIB(void* object)
{
    ArithInstrBuffer* AIB = (ArithInstrBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetClocked();

dbg(AIB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_INB(void* object);
class InstructionBuffer : public Buffer<InstructionDecode, InstructionDecode>
{
public:
    friend void* thread_INB(void* object);
    InstructionBuffer() : Buffer(thread_INB){}

    string GetContent()
    {
        string content = "INB:";
        if(!isEmpty)
        {
            for(int i=head; i<numInstr; i++)
            {
                content += instructions[i].ToString();
                if(i!=(numInstr-1))
                    content += ",";
            }
        }
        return content;
    }

private:
    InstructionDecode instructions[16];
    int head;
    int numInstr;

    void Initialize()
    {
        head = numInstr = 0;
    }

    /******************************************/
    InstructionDecode ReadData() {return instructions[head];}

    /******************************************/
    void WriteData() {instructions[head+numInstr] = tempInput;}

    /******************************************/
    InstructionDecode TestRead() {return ReadData();}
};

void* thread_INB(void* object)
{
    InstructionBuffer* INB = (InstructionBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetClocked();

dbg(INB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_ADB(void* object);
class AddressBuffer : public Buffer<RegisterValue, RegisterValue>
{
public:
    friend void* thread_ADB(void* object);
    AddressBuffer() : Buffer(thread_ADB){}

    string GetContent()
    {
        if(!isEmpty)
            return "ADB:" + currentAddress.ToString();
        else
            return "ADB:";
    }

private:
    RegisterValue currentAddress;

    /******************************************/
    RegisterValue ReadData() {return currentAddress;}

    /******************************************/
    void WriteData() {currentAddress = tempInput;}

    /******************************************/
    RegisterValue TestRead() {return ReadData();}
};

void* thread_ADB(void* object)
{
    AddressBuffer* ADB = (AddressBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetClocked();

dbg(ADB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_DAM(void* object);
class DataMemory : public Buffer<MemoryValue, MemoryValue>
{
public:
    friend void* thread_DAM(void* object);
    DataMemory() : Buffer(thread_DAM){}

    string GetContent()
    {
        if(!isEmpty)
            return "DAM:" + currentAddress.ToString();
        else
            return "DAM:";
    }

private:
    MemoryValue dataMem[8];

    /******************************************/
    RegisterValue ReadData() {return currentAddress;}

    /******************************************/
    void WriteData() {currentAddress = tempInput;}

    /******************************************/
    RegisterValue TestRead() {return ReadData();}
};

/******************************************/
void* thread_DAM(void* object)
{
    DataMemory* DAM = (DataMemory*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetClocked();

dbg(DAM->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(5));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
class Transition
{
public:

protected:
    CycleThread* thread;

    virtual void StartThread() = 0;
};

/******************************************************************************************/
int main()
{
    LoadInstrBuffer LIB;
    ArithInstrBuffer AIB;
    InstructionBuffer INB;
    AddressBuffer ADB;

    for(int i=0; i<5; i++)
    {
        // Check if last cycle, if so notify threads to exit
        if(i==(numCycles-1))
            SetExecutionComplete();

        // Clock each thread with rising edge
        SetClockEdge(true);

        // Wait for clock to be registered
        while(!CheckClocked())
            this_thread::sleep_for(chrono::milliseconds(5));

        // Clock falling edge
        SetClockEdge(false);

        // Wait for each thread to complete its loop and register the falling clock edge
        while(!CheckCycleComplete())
            this_thread::sleep_for(chrono::milliseconds(5));

        // Reset globals
        cycleComplete = false;
        clockedComplete = false;
    }

    // Join all threads
    LIB.Join();
    AIB.Join();
    INB.Join();
    ADB.Join();

    return 0;
}
