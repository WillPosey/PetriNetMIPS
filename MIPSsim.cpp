#define DEBUG

#define ALL_COMPLETE 2

#define DAM_FILENAME "datamemory.txt"
#define INM_FILENAME "instructions.txt"
#define RGF_FILENAME "registers.txt"

#include <thread>
#include <chrono>
#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <cstdlib>

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
    string opcode = "";
    string destReg = "";
    string srcReg1 = "";
    string srcReg2 = "";

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
    string opcode = "";
    int destRegNum = 0;
    int srcReg1Num = 0;
    int srcReg2Num = 0;

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
    int location = 0;
    int value = 0;

    string ToString()
    {
        return "<" + to_string(this->location) + ","
                   + to_string(this->value) + ">";
    }
};

/**************************/
struct RegisterValue
{
    int regNum = 0;
    int value = 0;

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
    Buffer(thread_func_ptr thrPtr) : thread_func(thrPtr){isEmpty = true; StartThread();};

    /******************************************/
    bool Empty() { return isEmpty; }

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
    void NoRead()
    {
        readLock.lock();
        hasRead = true;
        readLock.unlock();
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

    virtual void Initialize() = 0;
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
    void Initialize() {}

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
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(LIB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
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
    void Initialize() {}

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
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(AIB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
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
    InstructionBuffer() : Buffer(thread_INB){Initialize();}

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
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(INB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
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
    void Initialize() {}

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
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(ADB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
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
    DataMemory() : Buffer(thread_DAM){Initialize();}

    string GetContent()
    {
        string content = "DAM:";
        for(int i=0; i<8; i++)
        {
            content += dataMem[i].ToString();
            if(i!=7)
                content += ",";
        }
        return content;
    }

    void SetLocation(int location) {currentLocation = location;}

private:
    MemoryValue dataMem[8];
    int currentLocation;

    /******************************************/
    void Initialize()
    {
        string line;
        ifstream damFile (DAM_FILENAME);
        if(damFile.is_open())
        {
            for(int i=0; i<8; i++)
            {
                getline(damFile, line);
                dataMem[i].location = i;
                dataMem[i].value = atoi(line.substr(3,line.length()-4).c_str());
            }
            damFile.close();
        }
        else
        {
            cout << "Error: Could not read data memory file" << endl;
            for(int i=0; i<8; i++)
            {
                dataMem[i].location = i;
                dataMem[i].value = 0;
            }
        }
        isEmpty = false;
    }

    /******************************************/
    MemoryValue ReadData() {return dataMem[currentLocation];}

    /******************************************/
    MemoryValue TestRead() {return ReadData();}

    /******************************************/
    void WriteData() {}
};

/******************************************/
void* thread_DAM(void* object)
{
    DataMemory* DAM = (DataMemory*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(DAM->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_RGF(void* object);
class RegisterFile : public Buffer<RegisterValue, RegisterValue*>
{
public:
    friend void* thread_RGF(void* object);
    RegisterFile() : Buffer(thread_RGF){Initialize();}

    string GetContent()
    {
        string content = "RGF:";
        for(int i=0; i<8; i++)
        {
            content += registers[i].ToString();
            if(i!=7)
                content += ",";
        }
        return content;
    }

    bool RegistersAvailable(int regNum1, int regNum2)
    {
        SetCurrentRegisters(regNum1, regNum2);
        return regAvail[regNum1] && regAvail[regNum2];
    }

    void SetCurrentRegisters(int regNum1, int regNum2) {readReg1 = regNum1; readReg2 = regNum2;}
    void SetWriteRegister(int writeRegNum) {writeReg = writeRegNum;}

private:
    RegisterValue registers[8];
    bool regAvail[8];
    int readReg1, readReg2;
    int writeReg;

    /******************************************/
    void Initialize()
    {
        string line;
        ifstream damFile (RGF_FILENAME);
        if(damFile.is_open())
        {
            for(int i=0; i<8; i++)
            {
                getline(damFile, line);
                registers[i].regNum = i;
                registers[i].value = atoi(line.substr(4,line.length()-5).c_str());
            }
            damFile.close();
        }
        else
        {
            cout << "Error: Could not read register file" << endl;
            for(int i=0; i<8; i++)
            {
                registers[i].regNum = i;
                registers[i].value = 0;
            }
        }
        isEmpty = false;
    }

    /******************************************/
    RegisterValue* ReadData()
    {
        RegisterValue* reg = new RegisterValue[2];
        reg[0] = registers[0];
        reg[1] = registers[1];
        if(!regAvail[0])
            reg[0].value = -1;
        if(!regAvail[1])
            reg[1].value = -1;
        return reg;
    }

    /******************************************/
    RegisterValue* TestRead() {return ReadData();}

    /******************************************/
    void WriteData() {registers[writeReg] = (RegisterValue)tempInput;}
};

/******************************************/
void* thread_RGF(void* object)
{
    RegisterFile* RGF = (RegisterFile*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(RGF->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_REB(void* object);
class ResultBuffer : public Buffer<RegisterValue*, RegisterValue>
{
public:
    friend void* thread_REB(void* object);
    ResultBuffer() : Buffer(thread_REB){Initialize();}

    string GetContent()
    {
        string content = "REB:";
        for(int i=head; i<numResults; i++)
        {
            content += results[i].ToString();
            if(i!=(head+numResults-1))
                content += ",";
        }
        return content;
    }

    int GetCount() {return numResults;}

private:
    RegisterValue results[16];
    int head;
    int numResults;

    /******************************************/
    void Initialize()
    {
        head = numResults = 0;
    }

    /******************************************/
    RegisterValue ReadData()
    {
        RegisterValue result = results[head];
        head++;
        numResults--;
        return result;
    }

    /******************************************/
    RegisterValue TestRead() {return results[head];}

    /******************************************/
    void WriteData()
    {
        RegisterValue* temp = (RegisterValue*)tempInput;// guaranteed to have a value (both empty, call to NoWrite)
        results[head+numResults] = temp[0];// guaranteed to have a value (both empty, call to NoWrite)
        numResults++;
        if(temp[1].regNum >= 0) // could be empty
        {
            results[head+numResults] = temp[1];
            numResults++;
        }
    }
};

/******************************************/
void* thread_REB(void* object)
{
    ResultBuffer* REB = (ResultBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(REB->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/******************************************************************************************/
void* thread_INM(void* object);
class InstructionMemory : public Buffer<InstructionString, InstructionString>
{
public:
    friend void* thread_INM(void* object);
    InstructionMemory() : Buffer(thread_INM){Initialize();}

    string GetContent()
    {
        string content = "INM:";
        for(int i=head; i<numInstr; i++)
        {
            content += instructions[i].ToString();
            if(i!=(head+numInstr-1))
                content += ",";
        }
        return content;
    }

private:
    InstructionString instructions[16];
    int head;
    int numInstr;

    /******************************************/
    void Initialize()
    {
        string line;
        ifstream inmFile (INM_FILENAME);
        head = numInstr = 0;
        if(inmFile.is_open())
        {
            while(getline(inmFile,line))
            {
                size_t firstComma = line.find_first_of(',');
                size_t secondComma = line.find(',',firstComma+1);
                size_t thirdComma = line.find(',',secondComma+1);
                size_t endLine = line.find('>');
                instructions[head+numInstr].opcode = line.substr(1,firstComma-1);
                instructions[head+numInstr].destReg = line.substr(firstComma+1,secondComma-(firstComma+1));
                instructions[head+numInstr].srcReg1 = line.substr(secondComma+1,thirdComma-(secondComma+1));
                instructions[head+numInstr].srcReg2 = line.substr(thirdComma+1,endLine-(thirdComma+1));
                numInstr++;
            }
        }
        else
            cout << "Error: Could not read register file" << endl;
        isEmpty = false;
    }

    /******************************************/
    InstructionString ReadData()
    {
        InstructionString instr = instructions[head];
        head++;
        if(head > numInstr)
            isEmpty = true;
        return instr;
    }

    /******************************************/
    InstructionString TestRead() {return instructions[head];}

    /******************************************/
    void WriteData() {}
};

/******************************************/
void* thread_INM(void* object)
{
    InstructionMemory* INM = (InstructionMemory*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

dbg(INM->GetContent());

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
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
    //Buffers
    InstructionMemory INM;
    LoadInstrBuffer LIB;
    ArithInstrBuffer AIB;
    InstructionBuffer INB;
    AddressBuffer ADB;
    DataMemory DAM;
    RegisterFile RGF;
    ResultBuffer REB;

    //Transitions
    //DecodeReadTransition DR;
    //Issue1Transition ISSUE1;
    //Issue2Transition ISSUE2;
    //AddrTransition ADDR;
    //AluTransition ALU;
    //LoadTransition LOAD;
    //WriteTransition WRITE;

    for(int i=0; i<5; i++)
    {
        // Check if last cycle, if so notify threads to exit
        /*if(INM.Empty() && LIB.Empty() && AIB.Empty() &&
             INB.Empty() && ADB.Empty() && DAM.Empty() &&
             RGF.Empty() && REB.GetCount() == 1
            )
                SetExecutionComplete();
        */
        if(i==(numCycles-1))
            SetExecutionComplete();

        // Clock each thread with rising edge
        SetClockEdge(true);

        // Wait for clock to be registered
        while(!CheckClocked())
            this_thread::sleep_for(chrono::milliseconds(1));

        // Clock falling edge
        SetClockEdge(false);

        // Wait for each thread to complete its loop and register the falling clock edge
        while(!CheckCycleComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        // Reset globals
        cycleComplete = false;
        clockedComplete = false;
    }

    // Join all threads
    INM.Join();
    LIB.Join();
    AIB.Join();
    INB.Join();
    ADB.Join();
    DAM.Join();
    RGF.Join();
    REB.Join();
    //DR.Join();
    //ISSUE1.Join();
    //ISSUE2.Join();
    //ADDR.Join();
    //ALU.Join();
    //LOAD.Join();
    //WRITE.Join();


    return 0;
}
