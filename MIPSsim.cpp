#include <pthread.h>
#include <string>
#include <condition_variable>
#include <mutex>

using namespace std;

condition_variable clockSignal;
mutex clockLock;

/**************************/
struct InstructionString
{
    string opcode;
    string destReg;
    string srcReg1;
    string srcReg2;
};

/**************************/
struct InstructionDecode
{
    string opcode;
    int destRegNum;
    int srcReg1Num;
    int srcReg2Num;
};

/**************************/
struct MemoryValue
{
    int location;
    int value;
};

/**************************/
struct RegisterValue
{
    int regNum;
    int value;
};

/******************************************************************************************/
class CycleThread {
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
template <class I, class O>
class Buffer {
public:
    /******************************************/
    bool Empty()
    {
        if(isEmpty)
        {
            unique_lock<mutex> lock(readLock);
            hasRead.notify_one();
        }
        return isEmpty;
    }
    /******************************************/
    void NoWrite()
    {
        unique_lock<mutex> lock(writeLock);
        noWrite = true;
        hasWritten.notify_one();
    }
    /******************************************/
    void Write(I writeI)
    {
        unique_lock<mutex> lock(writeLock);
        noWrite = false;
        tempInput = writeI;
        hasWritten.notify_one();
    }
    /******************************************/
    O Read()
    {
        O outputData;
        unique_lock<mutex> lock(readLock);
        outputData = ReadData();
        hasRead.notify_one();
        return outputData;
    }
    /******************************************/
    virtual string GetContent() = 0;

protected:
    CycleThread* thread;
    I tempInput;
    bool isEmpty;
    bool noWrite;

    mutex writeLock, readLock;
    condition_variable hasRead, hasWritten;

    virtual void Initialize() = 0;
    virtual void StartThread() = 0;
    virtual O ReadData() = 0;
    virtual O TestReadData() = 0;
    virtual void WriteData() = 0;
};

/******************************************************************************************/
void* thread_LIB(void* object)
{

}

class LoadInstrBuffer : protected Buffer<InstructionString, int>
{
public:
    friend void* thread_LIB(void*);
    LoadInstrBuffer() : Buffer(){Initialize(); StartThread();}
    string GetContent() {return "hi";}
private:
    /******************************************/
    void Initialize()
    {

    }
    /******************************************/
    void StartThread()
    {

    }
    /******************************************/
    int ReadData()
    {

    }
    /******************************************/
    void WriteData()
    {

    }
    /******************************************/
    int TestReadData()
    {

    }
};

/******************************************************************************************/
class Transition {
public:

protected:
    CycleThread* thread;

    virtual void StartThread() = 0;
};

/******************************************************************************************/
int main()
{
    LoadInstrBuffer buf();
    return 0;
}
