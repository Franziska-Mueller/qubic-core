#pragma once

#include "platform/m256.h"
#include "platform/concurrency.h"
#include "platform/time.h"

#include "private_settings.h"
#include "system.h"
#include "network_core/peers.h"


// Fetches log
struct RequestLog
{
    unsigned long long passcode[4];
    unsigned long long fromID;
    unsigned long long toID; // inclusive

    enum {
        type = 44,
    };
};

// Returns buffered log; clears the buffer; make sure you fetch log quickly enough, if the buffer is overflown log stops being written into it till the node restart
struct RespondLog
{
    // Variable-size log;

    enum {
        type = 45,
    };
};


// Request logid ranges from tx hash
struct RequestLogIdRangeFromTx
{
    unsigned long long passcode[4];
    unsigned int tick;
    m256i txHash;

    enum {
        type = 46,
    };
};


// Response logid ranges from tx hash
struct ResponseLogIdRangeFromTx
{
    long long fromLogId;
    long long length;

    enum {
        type = 47,
    };
};


#define QU_TRANSFER 0
#define ASSET_ISSUANCE 1
#define ASSET_OWNERSHIP_CHANGE 2
#define ASSET_POSSESSION_CHANGE 3
#define CONTRACT_ERROR_MESSAGE 4
#define CONTRACT_WARNING_MESSAGE 5
#define CONTRACT_INFORMATION_MESSAGE 6
#define CONTRACT_DEBUG_MESSAGE 7
#define BURNING 8
#define CUSTOM_MESSAGE 255


/*
* STRUCTS FOR LOGGING
*/
struct QuTransfer
{
    m256i sourcePublicKey;
    m256i destinationPublicKey;
    long long amount;
#if LOG_QU_TRANSFERS && LOG_QU_TRANSFERS_TRACK_TRANSFER_ID
    long long transferId;
#endif
    char _terminator; // Only data before "_terminator" are logged
};

struct AssetIssuance
{
    m256i issuerPublicKey;
    long long numberOfShares;
    char name[7];
    char numberOfDecimalPlaces;
    char unitOfMeasurement[7];

    char _terminator; // Only data before "_terminator" are logged
};

struct AssetOwnershipChange
{
    m256i sourcePublicKey;
    m256i destinationPublicKey;
    m256i issuerPublicKey;
    long long numberOfShares;
    char name[7];
    char numberOfDecimalPlaces;
    char unitOfMeasurement[7];

    char _terminator; // Only data before "_terminator" are logged
};

struct AssetPossessionChange
{
    m256i sourcePublicKey;
    m256i destinationPublicKey;
    m256i issuerPublicKey;
    long long numberOfShares;
    char name[7];
    char numberOfDecimalPlaces;
    char unitOfMeasurement[7];

    char _terminator; // Only data before "_terminator" are logged
};

struct DummyContractErrorMessage
{
    unsigned int _contractIndex; // Auto-assigned, any previous value will be overwritten
    unsigned int _type; // Assign a random unique (per contract) number to distinguish messages of different types

    // Other data go here

    char _terminator; // Only data before "_terminator" are logged
};

struct DummyContractWarningMessage
{
    unsigned int _contractIndex; // Auto-assigned, any previous value will be overwritten
    unsigned int _type; // Assign a random unique (per contract) number to distinguish messages of different types

    // Other data go here

    char _terminator; // Only data before "_terminator" are logged
};

struct DummyContractInfoMessage
{
    unsigned int _contractIndex; // Auto-assigned, any previous value will be overwritten
    unsigned int _type; // Assign a random unique (per contract) number to distinguish messages of different types

    // Other data go here

    char _terminator; // Only data before "_terminator" are logged
};

struct DummyContractDebugMessage
{
    unsigned int _contractIndex; // Auto-assigned, any previous value will be overwritten
    unsigned int _type; // Assign a random unique (per contract) number to distinguish messages of different types

    // Other data go here

    char _terminator; // Only data before "_terminator" are logged
};

struct DummyCustomMessage
{
    unsigned long long _type; // Assign a random unique number to distinguish messages of different types

    // Other data go here

    char _terminator; // Only data before "_terminator" are logged
};

struct Burning
{
    m256i sourcePublicKey;
    long long amount;

    char _terminator; // Only data before "_terminator" are logged
};

/*
 * LOGGING IMPLEMENTATION
 */
#define LOG_BUFFER_SIZE 8589934592ULL // 8GiB
#define LOG_MAX_STORAGE_ENTRIES (LOG_BUFFER_SIZE / sizeof(QuTransfer)) // Adjustable: here we assume most of logs are just qu transfer
#define LOG_MAX_STORAGE_TICK 20000ULL
#define LOG_AVG_TX_PER_TICK 64ULL
#define LOG_TX_INFO_STORAGE (LOG_MAX_STORAGE_TICK*LOG_AVG_TX_PER_TICK)
class qLogger
{
public:
    struct BlobInfo
    {
        long long startIndex;
        long long length;
    };
    struct TxBufferInfo
    {
        m256i hash;
        BlobInfo info;
    };

    inline static char* logBuffer = NULL;
    inline static unsigned long long logBufferTail;
    inline static unsigned long long logId;
    inline static unsigned int tickBegin;
    inline static m256i currentTxHash;
    inline static unsigned int currentTick;
    inline static BlobInfo currentTxInfo;
    inline static volatile char logBufferLocks;

    // 5 special txs for 5 special events in qubic
    inline static m256i SC_INITIALIZE_TX = m256i(0, 0, 0, 0);
    inline static m256i SC_BEGIN_EPOCH_TX = m256i(0, 1, 0, 0);
    inline static m256i SC_BEGIN_TICK_TX = m256i(0, 2, 0, 0);
    inline static m256i SC_END_TICK_TX = m256i(0, 3, 0, 0);
    inline static m256i SC_END_EPOCH_TX = m256i(0, 4, 0, 0);
    // some utils
    static bool isProtocolTx(m256i hash)
    {
        return (hash.m256i_u64[0] == 0) && (hash.m256i_u64[2] == 0) && (hash.m256i_u64[3] == 0);
    }

    static unsigned long long getLogId(const char* ptr)
    {
        // first 16 bytes are date time and epoch tick info
        // next 8 bytes are logId
        return ((unsigned long long*)ptr)[2];
    }

    // Struct to map log buffer from log id    
    static struct mapLogIdToBuffer
    {
        inline static BlobInfo mapLogIdToBufferIndex[LOG_MAX_STORAGE_ENTRIES]; // x: index on buffer, y: length
        static void init()
        {
            BlobInfo null_blob{ -1,-1 };
            for (unsigned long long i = 0; i < LOG_MAX_STORAGE_ENTRIES; i++)
            {
                mapLogIdToBufferIndex[i] = null_blob;
            }
        }
        static long long getIndex(unsigned long long logId)
        {
            BlobInfo res = mapLogIdToBufferIndex[logId % LOG_MAX_STORAGE_ENTRIES];
            if (getLogId(logBuffer + res.startIndex) == logId)
            {
                return res.startIndex;
            }
            return -1;
        }

        static long long getLength(unsigned long long logId)
        {
            BlobInfo res = mapLogIdToBufferIndex[logId % LOG_MAX_STORAGE_ENTRIES];
            if (getLogId(logBuffer + res.startIndex) == logId)
            {
                return res.length;
            }
            return -1;
        }

        static BlobInfo getBlobInfo(unsigned long long logId)
        {
            BlobInfo res = mapLogIdToBufferIndex[logId % LOG_MAX_STORAGE_ENTRIES];
            if (getLogId(logBuffer + res.startIndex) == logId)
            {
                return res;
            }
            return BlobInfo{ -1,-1 };
        }

        static void set(unsigned long long logId, long long index, long long length)
        {
            BlobInfo& res = mapLogIdToBufferIndex[logId % LOG_MAX_STORAGE_ENTRIES];
            res.startIndex = index;
            res.length = length;
        }
    } logBuf;

    
    // Struct to map log id ranges from tx hash
    static struct mapTxToLogIdAccess
    {
        inline static TxBufferInfo mapTxToLogId[LOG_TX_INFO_STORAGE];
        inline static BlobInfo tickIndex[MAX_NUMBER_OF_TICKS_PER_EPOCH];
        inline static unsigned long long mapTxToLogIdCounter;
        static void init()
        {
            BlobInfo null_blob{ -1,-1 };
            for (unsigned long long i = 0; i < MAX_NUMBER_OF_TICKS_PER_EPOCH; i++)
            {
                tickIndex[i] = null_blob;
            }
            for (unsigned long long i = 0; i < LOG_TX_INFO_STORAGE; i++)
            {
                mapTxToLogId[i].hash = _mm256_setzero_si256();
                mapTxToLogId[i].info = null_blob;
            }
            mapTxToLogIdCounter = 0;
        }

        // return the logID ranges of a tx hash
        static BlobInfo getLogIdInfo(unsigned int tick, m256i txHash)
        {
            unsigned int tickOffset = tick - tickBegin;
            if (tickOffset < MAX_NUMBER_OF_TICKS_PER_EPOCH)
            {
                unsigned long long start = tickIndex[tickOffset].startIndex;
                unsigned long long end = start + tickIndex[tickOffset].length;
                for (unsigned long long i = start; i < end; i++)
                {
                    if (mapTxToLogId[i].hash == txHash)
                    {
                        return mapTxToLogId[i].info;
                    }
                }
            }            
            return BlobInfo{ -1,-1 };
        }

        static void _registerNewTx(unsigned int tick, m256i txHash)
        {
            if (currentTick != tick || currentTxHash != txHash)
            {
                currentTick = tick;
                currentTxHash = txHash;
            }
        }

        static void addLogId()
        {
            unsigned long long offsetTick = currentTick - tickBegin;
            ASSERT(offsetTick < MAX_NUMBER_OF_TICKS_PER_EPOCH);

            if (mapTxToLogIdCounter == 0)
            {
                auto& txInfo = mapTxToLogId[0];
                txInfo.hash == currentTxHash;
                txInfo.info.startIndex = logId;
                txInfo.info.length = 1;
                tickIndex[offsetTick].startIndex = mapTxToLogIdCounter;
                tickIndex[offsetTick].length = 1;
                mapTxToLogIdCounter++;
                return;
            }

            auto& txInfo = mapTxToLogId[(mapTxToLogIdCounter - 1) % LOG_TX_INFO_STORAGE];
            if (txInfo.hash == currentTxHash)
            {
                ASSERT(txInfo.info.startIndex != -1);
                ASSERT(txInfo.info.length != -1);
                txInfo.info.length++;
            }
            else // new tx is registered and generates log
            {
                auto& newTxInfo = mapTxToLogId[mapTxToLogIdCounter % LOG_TX_INFO_STORAGE];
                newTxInfo.hash == currentTxHash;
                newTxInfo.info.startIndex = logId;
                newTxInfo.info.length = 1;
                if (tickIndex[offsetTick].startIndex == -1) // new tick
                {
                    tickIndex[offsetTick].startIndex = mapTxToLogIdCounter;
                    tickIndex[offsetTick].length = 1;
                }
                else
                {
                    ASSERT(tickIndex[offsetTick].startIndex != -1);
                    tickIndex[offsetTick].length++;
                }
                mapTxToLogIdCounter++;
            }
        }
    } tx;

    static void registerNewTx(unsigned int tick, m256i txHash)
    {
        tx._registerNewTx(tick, txHash);
    }


    static bool initLogging()
    {
#if LOG_QU_TRANSFERS | LOG_ASSET_ISSUANCES | LOG_ASSET_OWNERSHIP_CHANGES | LOG_ASSET_POSSESSION_CHANGES | LOG_CONTRACT_ERROR_MESSAGES | LOG_CONTRACT_WARNING_MESSAGES | LOG_CONTRACT_INFO_MESSAGES | LOG_CONTRACT_DEBUG_MESSAGES | LOG_CUSTOM_MESSAGES
        EFI_STATUS status;
        if (logBuffer == NULL)
        {
            if (status = bs->AllocatePool(EfiRuntimeServicesData, LOG_BUFFER_SIZE, (void**)&logBuffer))
            {
                logStatusToConsole(L"EFI_BOOT_SERVICES.AllocatePool() fails", status, __LINE__);

                return false;
            }
        }
        reset(0);
#endif
        return true;
    }
    static void deinitLogging()
    {
#if LOG_QU_TRANSFERS | LOG_ASSET_ISSUANCES | LOG_ASSET_OWNERSHIP_CHANGES | LOG_ASSET_POSSESSION_CHANGES | LOG_CONTRACT_ERROR_MESSAGES | LOG_CONTRACT_WARNING_MESSAGES | LOG_CONTRACT_INFO_MESSAGES | LOG_CONTRACT_DEBUG_MESSAGES | LOG_CUSTOM_MESSAGES
        freePool(logBuffer);
#endif
    }

    static void reset(unsigned int _tickBegin)
    {
        logBuf.init();
        tx.init();
        logBufferTail = 0;
        logId = 0;
        logBufferLocks = 0;
        tickBegin = _tickBegin;
    }

    static void logMessage(unsigned int messageSize, unsigned char messageType, const void* message)
    {
        ACQUIRE(logBufferLocks);
        if (logBufferTail + 16 + messageSize <= LOG_BUFFER_SIZE)
        {
            logBufferTail = 0; // reset back to beginning
        }
        *((unsigned char*)(logBuffer + (logBufferTail + 0))) = (unsigned char)(time.Year - 2000);
        *((unsigned char*)(logBuffer + (logBufferTail + 1))) = time.Month;
        *((unsigned char*)(logBuffer + (logBufferTail + 2))) = time.Day;
        *((unsigned char*)(logBuffer + (logBufferTail + 3))) = time.Hour;
        *((unsigned char*)(logBuffer + (logBufferTail + 4))) = time.Minute;
        *((unsigned char*)(logBuffer + (logBufferTail + 5))) = time.Second;

        *((unsigned short*)(logBuffer + (logBufferTail + 6))) = system.epoch;
        *((unsigned int*)(logBuffer + (logBufferTail + 8))) = system.tick;

        *((unsigned int*)(logBuffer + (logBufferTail + 12))) = messageSize | (messageType << 24);
        // add logId here
        *((unsigned long long*)(logBuffer + (logBufferTail + 16))) = logId++;
        copyMem(logBuffer + (logBufferTail + 24), message, messageSize);
        logBufferTail += 24 + messageSize;
        RELEASE(logBufferLocks);
    }

    template <typename T>
    void logQuTransfer(T message)
    {
#if LOG_QU_TRANSFERS
        logMessage(offsetof(T, _terminator), QU_TRANSFER, &message);
#endif
    }

    template <typename T>
    void logAssetIssuance(T message)
    {
#if LOG_ASSET_ISSUANCES
        logMessage(offsetof(T, _terminator), ASSET_ISSUANCE, &message);
#endif
    }

    template <typename T>
    void logAssetOwnershipChange(T message)
    {
#if LOG_ASSET_OWNERSHIP_CHANGES
        logMessage(offsetof(T, _terminator), ASSET_OWNERSHIP_CHANGE, &message);
#endif
    }

    template <typename T>
    void logAssetPossessionChange(T message)
    {
#if LOG_ASSET_POSSESSION_CHANGES
        logMessage(offsetof(T, _terminator), ASSET_POSSESSION_CHANGE, &message);
#endif
    }

    template <typename T>
    void __logContractErrorMessage(unsigned int contractIndex, T& message)
    {
        static_assert(offsetof(T, _terminator) >= 8, "Invalid contract error message structure");

#if LOG_CONTRACT_ERROR_MESSAGES
        * ((unsigned int*)&message) = contractIndex;
        logMessage(offsetof(T, _terminator), CONTRACT_ERROR_MESSAGE, &message);
#endif

        // In order to keep state changes consistent independently of (a) whether logging is enabled and
        // (b) potential compiler optimizes, set contractIndex to 0 after logging
        * ((unsigned int*)&message) = 0;
    }

    template <typename T>
    void __logContractWarningMessage(unsigned int contractIndex, T& message)
    {
        static_assert(offsetof(T, _terminator) >= 8, "Invalid contract warning message structure");

#if LOG_CONTRACT_WARNING_MESSAGES
        * ((unsigned int*)&message) = contractIndex;
        logMessage(offsetof(T, _terminator), CONTRACT_WARNING_MESSAGE, &message);
#endif

        // In order to keep state changes consistent independently of (a) whether logging is enabled and
        // (b) potential compiler optimizes, set contractIndex to 0 after logging
        * ((unsigned int*)&message) = 0;
    }

    template <typename T>
    void __logContractInfoMessage(unsigned int contractIndex, T& message)
    {
        static_assert(offsetof(T, _terminator) >= 8, "Invalid contract info message structure");

#if LOG_CONTRACT_INFO_MESSAGES
        * ((unsigned int*)&message) = contractIndex;
        logMessage(offsetof(T, _terminator), CONTRACT_INFORMATION_MESSAGE, &message);
#endif

        // In order to keep state changes consistent independently of (a) whether logging is enabled and
        // (b) potential compiler optimizes, set contractIndex to 0 after logging
        * ((unsigned int*)&message) = 0;
    }

    template <typename T>
    void __logContractDebugMessage(unsigned int contractIndex, T& message)
    {
        static_assert(offsetof(T, _terminator) >= 8, "Invalid contract debug message structure");

#if LOG_CONTRACT_DEBUG_MESSAGES
        * ((unsigned int*)&message) = contractIndex;
        logMessage(offsetof(T, _terminator), CONTRACT_DEBUG_MESSAGE, &message);
#endif

        // In order to keep state changes consistent independently of (a) whether logging is enabled and
        // (b) potential compiler optimizes, set contractIndex to 0 after logging
        * ((unsigned int*)&message) = 0;
    }

    template <typename T>
    void logBurning(T message)
    {
#if LOG_BURNINGS
        logMessage(offsetof(T, _terminator), BURNING, &message);
#endif
    }

    template <typename T>
    void logCustomMessage(T message)
    {
        static_assert(offsetof(T, _terminator) >= 8, "Invalid custom message structure");

#if LOG_CUSTOM_MESSAGES
        logMessage(offsetof(T, _terminator), CUSTOM_MESSAGE, &message);
#endif
    }
    // Request: ranges of log ID
    static void processRequestLog(Peer* peer, RequestResponseHeader* header)
    {
        RequestLog* request = header->getPayload<RequestLog>();
        if (request->passcode[0] == logReaderPasscodes[0]
            && request->passcode[1] == logReaderPasscodes[1]
            && request->passcode[2] == logReaderPasscodes[2]
            && request->passcode[3] == logReaderPasscodes[3])
        {
            ACQUIRE(logBufferLocks);
            BlobInfo startIdBufferRange = logBuf.getBlobInfo(request->fromID);
            BlobInfo endIdBufferRange = logBuf.getBlobInfo(request->toID); // inclusive
            if (startIdBufferRange.startIndex != -1 && startIdBufferRange.length != -1
                && endIdBufferRange.startIndex != -1 && endIdBufferRange.length != -1)
            {
                if (endIdBufferRange.startIndex < startIdBufferRange.startIndex)
                {
                    // round buffer case, response 2 packets
                    unsigned long long i = 0;
                    for (i = request->fromID; i <= request->toID; i++)
                    {
                        BlobInfo iBufferRange = logBuf.getBlobInfo(i);
                        if (iBufferRange.startIndex < startIdBufferRange.startIndex)
                        {
                            i--;
                            break;
                        }
                    }                    
                    // first packet: from startID to end of buffer
                    {
                        BlobInfo iBufferRange = logBuf.getBlobInfo(i);
                        unsigned long long startFrom = startIdBufferRange.startIndex;
                        unsigned long long length = iBufferRange.length + iBufferRange.startIndex - startFrom;
                        if (length > RequestResponseHeader::max_size)
                        {
                            length = RequestResponseHeader::max_size;
                        }
                        enqueueResponse(peer, (unsigned int)(length), RespondLog::type, header->dejavu(), logBuffer + startFrom);
                    }
                    // second packet: from start buffer to endID
                    {
                        unsigned long long startFrom = 0;
                        unsigned long long length = endIdBufferRange.length + endIdBufferRange.startIndex - startFrom;
                        if (length > RequestResponseHeader::max_size)
                        {
                            length = RequestResponseHeader::max_size;
                        }
                        enqueueResponse(peer, (unsigned int)(length), RespondLog::type, header->dejavu(), logBuffer + startFrom);
                    }
                }
                else
                {
                    unsigned long long startFrom = startIdBufferRange.startIndex;
                    unsigned long long length = endIdBufferRange.length + endIdBufferRange.startIndex - startFrom;
                    if (length > RequestResponseHeader::max_size)
                    {
                        length = RequestResponseHeader::max_size;
                    }
                    enqueueResponse(peer, (unsigned int)(length), RespondLog::type, header->dejavu(), logBuffer + startFrom);
                }
            }
            else
            {
                enqueueResponse(peer, 0, RespondLog::type, header->dejavu(), NULL);
            }
            RELEASE(logBufferLocks);
            return;
        }

        enqueueResponse(peer, 0, RespondLog::type, header->dejavu(), NULL);
    }

    void processRequestTxLogInfo(Peer* peer, RequestResponseHeader* header)
    {
        RequestLogIdRangeFromTx* request = header->getPayload<RequestLogIdRangeFromTx>();
        if (request->passcode[0] == logReaderPasscodes[0]
            && request->passcode[1] == logReaderPasscodes[1]
            && request->passcode[2] == logReaderPasscodes[2]
            && request->passcode[3] == logReaderPasscodes[3])
        {
            ACQUIRE(logBufferLocks);
            ResponseLogIdRangeFromTx resp;
            BlobInfo info = tx.getLogIdInfo(request->tick, request->txHash);
            resp.fromLogId = info.startIndex;
            resp.length = info.length;
            enqueueResponse(peer, sizeof(ResponseLogIdRangeFromTx), ResponseLogIdRangeFromTx::type, header->dejavu(), &resp);
            RELEASE(logBufferLocks);
            return;
        }

        enqueueResponse(peer, 0, RespondLog::type, header->dejavu(), NULL);
    }
};

qLogger logger;

// For smartcontract logging
template <typename T> void __logContractDebugMessage(unsigned int size, T& msg)
{
    logger.__logContractDebugMessage(size, msg);
}
template <typename T> void __logContractErrorMessage(unsigned int size, T& msg)
{
    logger.__logContractErrorMessage(size, msg);
}
template <typename T> void __logContractInfoMessage(unsigned int size, T& msg)
{
    logger.__logContractInfoMessage(size, msg);
}
template <typename T> void __logContractWarningMessage(unsigned int size, T& msg)
{
    logger.__logContractWarningMessage(size, msg);
}