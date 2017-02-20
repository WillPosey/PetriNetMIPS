#define ALL_COMPLETE 15

#define DAM_FILENAME "datamemory.txt"
#define INM_FILENAME "instructions.txt"
#define RGF_FILENAME "registers.txt"
#define SIM_FILENAME "simulation.txt"

#include <thread>
#include <chrono>
#include <string>
#include <mutex>
#include <iostream>
#include <fstream>
#include <cstdlib>

using namespace std;

/*** Thread function pointer ***/
typedef void* (*thread_func_ptr)(void*);

/***** Buffer Threads *****/
void* thread_LIB(void* object);
void* thread_AIB(void* object);
void* thread_INB(void* object);
void* thread_ADB(void* object);
void* thread_DAM(void* object);
void* thread_RGF(void* object);
void* thread_REB(void* object);
void* thread_INM(void* object);

/***** Transition Threads *****/
void* thread_DR(void* object);
void* thread_ISSUE1(void* object);
void* thread_ISSUE2(void* object);
void* thread_ADDR(void* object);
void* thread_LOAD(void* object);
void* thread_ALU(void* object);
void* thread_WRITE(void* object);

/*************** Locks and Access functions for Clocking Cycles ***************/

/*****************************************/
bool clockEdge = false;
mutex clockMutex;

bool CheckClockRisingEdge()
{
    bool risingEdge;
    clockMutex.lock();
    risingEdge = clockEdge;
    clockMutex.unlock();
    return risingEdge;
}

bool CheckClockFallingEdge()
{
    bool fallingEdge;
    clockMutex.lock();
    fallingEdge = !clockEdge;
    clockMutex.unlock();
    return fallingEdge;
}

void SetClockEdge(bool risingEdge)
{
    clockMutex.lock();
    clockEdge = risingEdge;
    clockMutex.unlock();
}

/*****************************************/
int countClockedComplete = 0;
bool clockedComplete = false;
mutex clockedCompleteMutex;

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

bool CheckClocked()
{
    bool clocked;
    clockedCompleteMutex.lock();
    clocked = clockedComplete;
    clockedCompleteMutex.unlock();
    return clocked;
}

/*****************************************/
int countCycleComplete = 0;
bool cycleComplete = false;
mutex cycleMutex;

void SetCycleComplete()
{
    cycleMutex.lock();
    countCycleComplete++;
    if(countCycleComplete == ALL_COMPLETE)
    {
        countCycleComplete = 0;
        cycleComplete = true;
    }
    cycleMutex.unlock();
}

bool CheckCycleComplete()
{
    bool complete;
    cycleMutex.lock();
    complete = cycleComplete;
    cycleMutex.unlock();
    return complete;
}

/*****************************************/
bool executionComplete = false;
mutex completeMutex;

void SetExecutionComplete()
{
    completeMutex.lock();
    executionComplete = true;
    completeMutex.unlock();
}

bool CheckExecutionComplete()
{
    bool complete;
    completeMutex.lock();
    complete = executionComplete;
    completeMutex.unlock();
    return complete;
}


/*************** Lock and Access Functions for Issue Transition Synchronization ***************/
mutex issueLock;
bool bothIssueRead = false;
int issueRead = 0;

void SetIssueRead()
{
    issueLock.lock();
    bothIssueRead = false;
    issueRead++;
    if(issueRead == 2)
    {
        bothIssueRead = true;
        issueRead = 0;
    }
    issueLock.unlock();
}

bool CheckIssueRead()
{
    bool bothRead;
    issueLock.lock();
    bothRead = bothIssueRead;
    issueLock.unlock();
    return bothRead;
}

/************************** Structs **************************/
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
                   + "R" + to_string(destRegNum) + ","
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

/************************** CycleThread Class **************************/
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

/************************** Abstract Base Buffer Class **************************/
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
    virtual Out TestRead() = 0;

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

    void ResetLocks() {hasRead = hasWritten = false;}

    virtual void StartThread(){thread = new CycleThread(thread_func, (void*) this);};
    virtual void Initialize() = 0;
    virtual Out ReadData() = 0;
    virtual void WriteData() = 0;
};

/************************** Derived Buffer Classes and corresponding thread functions **************************/
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
    InstructionDecode ReadData() { isEmpty = true; return currentInstr;}

    /******************************************/
    void WriteData() {currentInstr = (InstructionDecode)tempInput; isEmpty = false;}

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

        while(!LIB->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        while(!LIB->WriteComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        if(!LIB->noWrite)
            LIB->WriteData();

        LIB->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
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
    InstructionDecode ReadData() { isEmpty = true; return currentInstr;}

    /******************************************/
    void WriteData() {currentInstr = (InstructionDecode)tempInput; isEmpty = false;}

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

        while(!AIB->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        while(!AIB->WriteComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        if(!AIB->noWrite)
            AIB->WriteData();
        AIB->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
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
            for(int i=head; i<(head+numInstr); i++)
            {
                content += instructions[i].ToString();
                if(i!=(head+numInstr-1))
                    content += ",";
            }
        }
        return content;
    }

    /******************************************/
    InstructionDecode TestRead() {return instructions[head];}

private:
    InstructionDecode instructions[16];
    int head;
    int numInstr;

    void Initialize()
    {
        head = numInstr = 0;
    }

    /******************************************/
    InstructionDecode ReadData()
    {
        InstructionDecode instr = instructions[head];
        head++;
        numInstr--;
        if(head >= (head+numInstr))
            isEmpty = true;
        return instr;
    }

    /******************************************/
    void WriteData()
    {
        instructions[head+numInstr] = (InstructionDecode)tempInput;
        numInstr++;
        isEmpty = false;
    }
};

void* thread_INB(void* object)
{
    InstructionBuffer* INB = (InstructionBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        while(!INB->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        while(!INB->WriteComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        if(!INB->noWrite)
            INB->WriteData();

        INB->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
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

    /******************************************/
    RegisterValue TestRead() {return ReadData();}

private:
    RegisterValue currentAddress;

    /******************************************/
    void Initialize() {}

    /******************************************/
    RegisterValue ReadData() {isEmpty = true; return currentAddress;}

    /******************************************/
    void WriteData() {currentAddress = (RegisterValue)tempInput; isEmpty = false;}

};

void* thread_ADB(void* object)
{
    AddressBuffer* ADB = (AddressBuffer*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        while(!ADB->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        while(!ADB->WriteComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        if(!ADB->noWrite)
            ADB->WriteData();

        ADB->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
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

    /******************************************/
    MemoryValue TestRead() {return ReadData();}

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

        while(!DAM->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        DAM->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
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
    RegisterValue* TestRead() {return ReadData();}

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
                regAvail[i] = true;
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
        reg[0] = registers[readReg1];
        reg[1] = registers[readReg2];
        if(!regAvail[readReg1])
            reg[0].value = -1;
        if(!regAvail[readReg2])
            reg[1].value = -1;
        return reg;
    }

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

        while(!RGF->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        while(!RGF->WriteComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        if(!RGF->noWrite)
            RGF->WriteData();

        RGF->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class ResultBuffer : public Buffer<RegisterValue, RegisterValue>
{
public:
    friend void* thread_REB(void* object);
    ResultBuffer() : Buffer(thread_REB){Initialize();}

    string GetContent()
    {
        string content = "REB:";
        for(int i=head; i<(head+numResults); i++)
        {
            content += results[i].ToString();
            if(i!=(head+numResults-1))
                content += ",";
        }
        return content;
    }

    RegisterValue TestRead() {return results[head];}

     /******************************************/
    void NoWrite(int resultType)
    {
        writeLock.lock();
        writeCount++;

        if(resultType)
            noWrite2 = true;
        else
            noWrite1 = true;

        if(writeCount==2)
            hasWritten = true;

        writeLock.unlock();
    }
    /******************************************/
    void Write(RegisterValue writeInput, int resultType)
    {
        writeLock.lock();
        writeCount++;

        if(resultType)
        {
            noWrite2 = false;
            result2 = (RegisterValue) writeInput;
        }
        else
        {
            noWrite1 = false;
            result1 = (RegisterValue) writeInput;
        }

        if(writeCount==2)
            hasWritten = true;

        writeLock.unlock();
    }

private:
    RegisterValue results[16];
    RegisterValue result1;
    RegisterValue result2;
    bool noWrite1;
    bool noWrite2;
    int writeCount;
    int head;
    int numResults;

    /******************************************/
    void Initialize()
    {
        head = numResults = writeCount = 0;
    }

    /******************************************/
    RegisterValue ReadData()
    {
        RegisterValue result = results[head];
        head++;
        numResults--;
        if(head >= (head+numResults))
            isEmpty = true;
        return result;
    }

    /******************************************/
    void WriteData()
    {
        if(!noWrite1)
        {
            results[head+numResults] = result1;
            numResults++;
            isEmpty = false;
        }
        if(!noWrite2)
        {
            results[head+numResults] = result2;
            numResults++;
            isEmpty = false;
        }
        writeCount = 0;
        noWrite1 = false;
        noWrite2 = false;
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

        while(!REB->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        while(!REB->WriteComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        REB->WriteData();

        REB->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class InstructionMemory : public Buffer<InstructionString, InstructionString>
{
public:
    friend void* thread_INM(void* object);
    InstructionMemory() : Buffer(thread_INM){Initialize();}

    string GetContent()
    {
        string content = "INM:";
        for(int i=head; i<(head+numInstr); i++)
        {
            content += instructions[i].ToString();
            if(i!=(head+numInstr-1))
                content += ",";
        }
        return content;
    }

    InstructionString TestRead() {return instructions[head];}

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
                instructions[numInstr].opcode = line.substr(1,firstComma-1);
                instructions[numInstr].destReg = line.substr(firstComma+1,secondComma-(firstComma+1));
                instructions[numInstr].srcReg1 = line.substr(secondComma+1,thirdComma-(secondComma+1));
                instructions[numInstr].srcReg2 = line.substr(thirdComma+1,endLine-(thirdComma+1));
                numInstr++;
            }
        }
        else
            cout << "Error: Could not read instructions file" << endl;
        isEmpty = false;
    }

    /******************************************/
    InstructionString ReadData()
    {
        InstructionString instr = instructions[head];
        head++;
        numInstr--;
        if(head >= (head+numInstr))
            isEmpty = true;
        return instr;
    }

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

        while(!INM->ReadComplete())
            this_thread::sleep_for(chrono::milliseconds(1));

        INM->ResetLocks();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/************************** Abstract Base Transition Class ********************************/
class Transition
{
public:
    Transition(thread_func_ptr thrPtr) : thread_func(thrPtr) {StartThread();}
    void Join(){ thread->JoinThread(); }
protected:
    CycleThread* thread;
    thread_func_ptr thread_func;

    void StartThread(){thread = new CycleThread(thread_func, (void*) this);};
};

/************************* Derived Transition Classes and thread functions ********************************/
class DecodeReadTransition : public Transition
{
public:
    friend void* thread_DR(void* object);
    DecodeReadTransition(InstructionMemory& INMref, RegisterFile& RGFref, InstructionBuffer& INBref) :
                         INM(INMref),
                         RGF(RGFref),
                         INB(INBref),
                         Transition(thread_DR){INM_Empty = false;};
private:
    InstructionMemory& INM;
    RegisterFile& RGF;
    InstructionBuffer& INB;

    InstructionString fetchedInstruction;
    RegisterValue srcReg1;
    RegisterValue srcReg2;
    int srcReg1Num, srcReg2Num;
    InstructionDecode decodedInstruction;
    bool INM_Empty;

    void FetchInstruction()
    {
        if(INM.Empty())
        {
            INM.NoRead();
            INM_Empty = true;
        }
        else
        {
            fetchedInstruction = INM.Read();
            srcReg1Num = atoi(fetchedInstruction.srcReg1.substr(1,1).c_str());
            srcReg2Num = atoi(fetchedInstruction.srcReg2.substr(1,1).c_str());
            INM_Empty = false;
        }
    }

    void ReadRegisters()
    {
        if(INM_Empty)
        {
            RGF.NoRead();
        }
        else
        {
            if(RGF.RegistersAvailable(srcReg1Num, srcReg2Num))
            {
                RegisterValue* reg = RGF.Read();
                srcReg1 = reg[0];
                srcReg2 = reg[1];
                decodedInstruction.opcode = fetchedInstruction.opcode;
                decodedInstruction.destRegNum = atoi(fetchedInstruction.destReg.substr(1,1).c_str());
                decodedInstruction.srcReg1Num = srcReg1.value;
                decodedInstruction.srcReg2Num = srcReg2.value;
            }
        }
    }

    void WriteToBuffer()
    {
        if(INM_Empty)
            INB.NoWrite();
        else
            INB.Write(decodedInstruction);
    }
};

/******************************************/
void* thread_DR(void* object)
{
    DecodeReadTransition* DR = (DecodeReadTransition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        DR->FetchInstruction();
        DR->ReadRegisters();
        DR->WriteToBuffer();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class Issue1Transition : public Transition
{
public:
    friend void* thread_ISSUE1(void* object);
    Issue1Transition(InstructionBuffer& INBref, ArithInstrBuffer& AIBref) :
                     INB(INBref),
                     AIB(AIBref),
                     Transition(thread_ISSUE1){};
private:
    InstructionBuffer& INB;
    ArithInstrBuffer& AIB;

    InstructionDecode decodedInstruction;
    bool correctType;
    bool INB_Empty;
    string ADD_op = "ADD", SUB_op = "SUB", AND_op = "AND", OR_op = "OR";

    void ReadInstruction()
    {
        string op;
        INB_Empty = INB.Empty();

        if(!INB_Empty)
        {
            decodedInstruction = INB.TestRead();
            op = decodedInstruction.opcode;
            correctType = (op.compare(ADD_op) == 0) ||
                          (op.compare(SUB_op) == 0) ||
                          (op.compare(AND_op) == 0) ||
                          (op.compare(OR_op) == 0);
        }
    }

    void IssueInstruction()
    {
        if(!INB_Empty && correctType)
        {
            decodedInstruction = INB.Read();
            AIB.Write(decodedInstruction);
        }
        else
        {
            INB.NoRead();
            AIB.NoWrite();
        }
    }
};

/******************************************/
void* thread_ISSUE1(void* object)
{
    Issue1Transition* ISSUE1 = (Issue1Transition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        ISSUE1->ReadInstruction();
        SetIssueRead();
        while(!CheckIssueRead())
            this_thread::sleep_for(chrono::milliseconds(1));
        ISSUE1->IssueInstruction();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class Issue2Transition : public Transition
{
public:
    friend void* thread_ISSUE2(void* object);
    Issue2Transition(InstructionBuffer& INBref, LoadInstrBuffer& LIBref) :
                         INB(INBref),
                         LIB(LIBref),
                         Transition(thread_ISSUE2){};
private:
    InstructionBuffer& INB;
    LoadInstrBuffer& LIB;

    InstructionDecode decodedInstruction;
    bool correctType;
    bool INB_Empty;
    string LD_op = "LD";

    void ReadInstruction()
    {
        string op;
        INB_Empty = INB.Empty();

        if(!INB_Empty)
        {
            decodedInstruction = INB.TestRead();
            op = decodedInstruction.opcode;
            correctType = (op.compare(LD_op) == 0) ;
        }
    }

    void IssueInstruction()
    {
        if(!INB_Empty && correctType)
        {
            decodedInstruction = INB.Read();
            LIB.Write(decodedInstruction);
        }
        else
        {
            INB.NoRead();
            LIB.NoWrite();
        }
    }
};

/******************************************/
void* thread_ISSUE2(void* object)
{
    Issue2Transition* ISSUE2 = (Issue2Transition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        ISSUE2->ReadInstruction();
        SetIssueRead();
        while(!CheckIssueRead())
            this_thread::sleep_for(chrono::milliseconds(1));
        ISSUE2->IssueInstruction();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class AddrTransition : public Transition
{
public:
    friend void* thread_ADDR(void* object);
    AddrTransition( LoadInstrBuffer& LIBref, AddressBuffer& ADBref) :
                    LIB(LIBref),
                    ADB(ADBref),
                    Transition(thread_ADDR){};
private:
    LoadInstrBuffer& LIB;
    AddressBuffer& ADB;

    InstructionDecode decodedInstruction;
    RegisterValue registerAddress;
    bool LIB_Empty;

    void ReadInstruction()
    {
        LIB_Empty = LIB.Empty();

        if(!LIB_Empty)
        {
            decodedInstruction = LIB.Read();
            registerAddress.regNum = decodedInstruction.destRegNum;
            registerAddress.value = decodedInstruction.srcReg1Num + decodedInstruction.srcReg2Num;
        }
        else
            LIB.NoRead();
    }

    void WriteRegisterAddress()
    {
        if(!LIB_Empty)
            ADB.Write(registerAddress);
        else
            ADB.NoWrite();
    }
};

/******************************************/
void* thread_ADDR(void* object)
{
    AddrTransition* ADDR = (AddrTransition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        ADDR->ReadInstruction();
        ADDR->WriteRegisterAddress();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class LoadTransition : public Transition
{
public:
    friend void* thread_LOAD(void* object);
    LoadTransition( DataMemory& DAMref, AddressBuffer& ADBref, ResultBuffer& REBref) :
                    DAM(DAMref),
                    ADB(ADBref),
                    REB(REBref),
                    Transition(thread_LOAD){};
private:
    DataMemory& DAM;
    AddressBuffer& ADB;
    ResultBuffer& REB;

    MemoryValue memory;
    RegisterValue registerAddress;
    RegisterValue registerResult;
    bool ADB_Empty;

    void LoadResult()
    {
        ADB_Empty = ADB.Empty();

        if(!ADB_Empty)
        {
            registerAddress = ADB.Read();
            DAM.SetLocation(registerAddress.value);
            memory = DAM.Read();
            registerResult.regNum = registerAddress.regNum;
            registerResult.value = memory.value;
        }
        else
        {
            ADB.NoRead();
            DAM.NoRead();
        }
    }

    void WriteResultBuffer()
    {
        if(!ADB_Empty)
            REB.Write(registerResult, 0);
        else
            REB.NoWrite(0);
    }
};

/******************************************/
void* thread_LOAD(void* object)
{
    LoadTransition* LOAD = (LoadTransition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        LOAD->LoadResult();
        LOAD->WriteResultBuffer();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class AluTransition : public Transition
{
public:
    friend void* thread_ALU(void* object);
    AluTransition( ArithInstrBuffer& AIBref, ResultBuffer& REBref) :
                    AIB(AIBref),
                    REB(REBref),
                    Transition(thread_ALU){};
private:
    ArithInstrBuffer& AIB;
    ResultBuffer& REB;

    InstructionDecode instruction;
    RegisterValue registerResult;
    int result, operand1, operand2;
    bool AIB_Empty;
    string ADD_op = "ADD", SUB_op = "SUB", AND_op = "AND", OR_op = "OR";

    void CalculateResult()
    {
        string op;
        AIB_Empty = AIB.Empty();

        if(!AIB_Empty)
        {
            instruction = AIB.Read();
            op = instruction.opcode;
            operand1 = instruction.srcReg1Num;
            operand2 = instruction.srcReg2Num;
            if(op.compare(ADD_op) == 0)
                result = (unsigned) (operand1 + operand2);
            else if(op.compare(SUB_op) == 0)
                result = (unsigned) (operand1 - operand2);
            else if(op.compare(AND_op) == 0)
                result = (unsigned) (operand1 & operand2);
            else if(op.compare(OR_op) == 0)
                result = (unsigned) (operand1 | operand2);
            registerResult.regNum = instruction.destRegNum;
            registerResult.value = result;
        }
        else
            AIB.NoRead();
    }

    void WriteResultBuffer()
    {
        if(!AIB_Empty)
            REB.Write(registerResult, 1);
        else
            REB.NoWrite(1);
    }
};

/******************************************/
void* thread_ALU(void* object)
{
    AluTransition* ALU = (AluTransition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        ALU->CalculateResult();
        ALU->WriteResultBuffer();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/****************************************************************************/
class WriteTransition : public Transition
{
public:
    friend void* thread_WRITE(void* object);
    WriteTransition(ResultBuffer& REBref, RegisterFile& RGFref) :
                    REB(REBref),
                    RGF(RGFref),
                    Transition(thread_WRITE){};
private:
    ResultBuffer& REB;
    RegisterFile& RGF;

    RegisterValue registerResult;
    bool REB_Empty;

    void ReadResult()
    {
        string op;
        REB_Empty = REB.Empty();

        if(!REB_Empty)
            registerResult = REB.Read();
        else
            REB.NoRead();
    }

    void WriteToRegisterFile()
    {
        if(!REB_Empty)
        {
            RGF.SetWriteRegister(registerResult.regNum);
            RGF.Write(registerResult);
        }
        else
            RGF.NoWrite();
    }
};

/******************************************/
void* thread_WRITE(void* object)
{
    WriteTransition* WRITE = (WriteTransition*) object;

    while(1)
    {
        while(!CheckClockRisingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetClocked();

        WRITE->ReadResult();
        WRITE->WriteToRegisterFile();

        while(!CheckClockFallingEdge())
            this_thread::sleep_for(chrono::milliseconds(1));
        SetCycleComplete();
        if(CheckExecutionComplete())
            break;
    }
}

/**************************** Main Method ****************************************/
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
    DecodeReadTransition DR (INM, RGF, INB);
    Issue1Transition ISSUE1 (INB, AIB);
    Issue2Transition ISSUE2 (INB, LIB);
    AddrTransition ADDR (LIB, ADB);
    AluTransition ALU (AIB, REB);
    LoadTransition LOAD (DAM, ADB, REB);
    WriteTransition WRITE (REB, RGF);

    int stepNum = 0;
    bool lastStep = false;
    ofstream outfile (SIM_FILENAME);

    while(!lastStep)
    {
        // Check if last cycle, if so notify threads to exit
        lastStep = (INM.Empty() && LIB.Empty() && AIB.Empty() &&
                    INB.Empty() && ADB.Empty() && REB.Empty()
                   );

        if(lastStep)
            SetExecutionComplete();

        //Get Content of each Buffer
        outfile <<  "STEP " << stepNum << ":" << "\r\n" <<
                    INM.GetContent() << "\r\n" <<
                    INB.GetContent() << "\r\n" <<
                    AIB.GetContent() << "\r\n" <<
                    LIB.GetContent() << "\r\n" <<
                    ADB.GetContent() << "\r\n" <<
                    REB.GetContent() << "\r\n" <<
                    RGF.GetContent() << "\r\n" <<
                    DAM.GetContent() << "\r\n";
        if(!lastStep)
            outfile << "\r\n";

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
        stepNum++;
    }

    outfile.close();

    // Join all threads
    INM.Join();
    DR.Join();
    INB.Join();
    ISSUE1.Join();
    ISSUE2.Join();
    LIB.Join();
    AIB.Join();
    ADDR.Join();
    ADB.Join();
    DAM.Join();
    ALU.Join();
    LOAD.Join();
    REB.Join();
    WRITE.Join();
    RGF.Join();

    return 0;
}
