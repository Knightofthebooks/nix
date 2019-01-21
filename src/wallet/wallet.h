// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLET_H
#define BITCOIN_WALLET_WALLET_H

#include <amount.h>
#include <outputtype.h>
#include <policy/feerate.h>
#include <streams.h>
#include <tinyformat.h>
#include <ui_interface.h>
#include <utilstrencodings.h>
#include <validationinterface.h>
#include <script/ismine.h>
#include <script/sign.h>
#include <wallet/crypter.h>
#include <wallet/walletdb.h>
#include <wallet/rpcwallet.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>
#include "zerocoin/zerocoin.h"
#include <ghost-address/extkey.h>
#include <ghost-address/stealth.h>
#include <crypto/sha512.h>
#include "../base58.h"
#include <crypto/hmac_sha256.h>
#include <crypto/hmac_sha512.h>
#include <miner.h>
#include <univalue/include/univalue.h>

const uint256 ABANDON_HASH;
bool AddWallet(const std::shared_ptr<CWallet>& wallet);
bool RemoveWallet(const std::shared_ptr<CWallet>& wallet);
bool HasWallets();
std::vector<std::shared_ptr<CWallet>> GetWallets();
std::shared_ptr<CWallet> GetWallet(const std::string& name);

static const unsigned int DEFAULT_KEYPOOL_SIZE = 1000;
//! -paytxfee default
constexpr CAmount DEFAULT_PAY_TX_FEE = 0;
//! -fallbackfee default
static const CAmount DEFAULT_FALLBACK_FEE = 20000;
//! -discardfee default
static const CAmount DEFAULT_DISCARD_FEE = 10000;
//! -mintxfee default
static const CAmount DEFAULT_TRANSACTION_MINFEE = 1000;
//! minimum recommended increment for BIP 125 replacement txs
static const CAmount WALLET_INCREMENTAL_RELAY_FEE = 5000;
//! target minimum change amount
static const CAmount MIN_CHANGE = CENT;
//! final minimum change amount after paying for fees
static const CAmount MIN_FINAL_CHANGE = MIN_CHANGE/2;
//! Default for -spendzeroconfchange
static const bool DEFAULT_SPEND_ZEROCONF_CHANGE = true;
//! Default for -walletrejectlongchains
static const bool DEFAULT_WALLET_REJECT_LONG_CHAINS = false;
//! Default for -avoidpartialspends
static const bool DEFAULT_AVOIDPARTIALSPENDS = false;
//! -txconfirmtarget default
static const unsigned int DEFAULT_TX_CONFIRM_TARGET = 6;
//! -walletrbf default
static const bool DEFAULT_WALLET_RBF = false;
static const bool DEFAULT_WALLETBROADCAST = true;
static const bool DEFAULT_DISABLE_WALLET = false;

//! Pre-calculated constants for input size estimation in *virtual size*
static constexpr size_t DUMMY_NESTED_P2WPKH_INPUT_SIZE = 91;

class CBlockIndex;
class CCoinControl;
class COutput;
class CReserveKey;
class CScript;
class CTxMemPool;
class CBlockPolicyEstimator;
class CWalletTx;
class CommitmentKeyPack;
struct FeeCalculation;
enum class FeeEstimateMode;

typedef std::map<uint256, CWalletTx> MapWallet_t;


const uint16_t PLACEHOLDER_N = 0xFFFF;

enum AvailableCoinsType
{
    ALL_COINS = 1,
    ONLY_DENOMINATED = 2,
    ONLY_NOT40000IFMN = 3,
    ONLY_NONDENOMINATED_NOT40000IFMN = 4,
    ONLY_40000 = 5, // find ghostnode outputs
    ONLY_PRIVATESEND_COLLATERAL = 6
};

struct CompactTallyItem
{
    CBitcoinAddress address;
    CAmount nAmount;
    std::vector<CTxIn> vecTxIn;
    CompactTallyItem()
    {
        nAmount = 0;
    }
};

enum OutputRecordFlags
{
    ORF_OWNED               = (1 << 0),
    ORF_FROM                = (1 << 1),
    ORF_CHANGE              = (1 << 2),
    ORF_SPENT               = (1 << 3),
    ORF_LOCKED              = (1 << 4), // Needs wallet to be unlocked for further processing
    ORF_STAKEONLY           = (1 << 5),
    ORF_WATCHONLY           = (1 << 6),
    ORF_HARDWARE_DEVICE     = (1 << 7),

    ORF_OWN_WATCH           =  ORF_STAKEONLY | ORF_WATCHONLY,
    ORF_OWN_ANY             = ORF_OWNED | ORF_OWN_WATCH,
};

enum OutputRecordAddressTypes
{
    ORA_EXTKEY       = 1,
    ORA_STEALTH      = 2,
    ORA_STANDARD     = 3,
};

class COutputRecord
{
public:
    COutputRecord() : nType(0), nFlags(0), n(0), nValue(-1) {};
    uint8_t nType;
    uint8_t nFlags;
    uint16_t n;
    CAmount nValue;
    CScript scriptPubKey;
    std::string sNarration;

    /*
    vPath 0 - ORA_EXTKEY
        1 - index to m
        2... path
    vPath 0 - ORA_STEALTH
        [1, 21] stealthkeyid
        [22, 55] pubkey (if not using ephemkey)
    vPath 0 - ORA_STANDARD
        [1, 34] pubkey
    */
    std::vector<uint8_t> vPath; // index to m is stored in first entry

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action)
    {
        READWRITE(nType);
        READWRITE(nFlags);
        READWRITE(n);
        READWRITE(nValue);
        READWRITE(*(CScriptBase*)(&scriptPubKey));
        READWRITE(sNarration);
        READWRITE(vPath);
    };
};

typedef std::map<uint8_t, std::vector<uint8_t> > mapRTxValue_t;
class CTransactionRecord
{
// Stored by uint256 txnHash;
public:
    CTransactionRecord() :
        nFlags(0), nIndex(0), nBlockTime(0) , nTimeReceived(0) , nFee(0) {};

    // Conflicted state is marked by set blockHash and nIndex -1
    uint256 blockHash;
    int16_t nFlags;
    int16_t nIndex;

    int64_t nBlockTime;
    int64_t nTimeReceived;
    CAmount nFee;
    mapRTxValue_t mapValue;

    std::vector<COutPoint> vin;
    std::vector<COutputRecord> vout;

    int InsertOutput(COutputRecord &r);
    bool EraseOutput(uint16_t n);

    COutputRecord *GetOutput(int n);
    const COutputRecord *GetOutput(int n) const;
    const COutputRecord *GetChangeOutput() const;

    void SetMerkleBranch(const uint256 &blockHash_, int posInBlock)
    {
        blockHash = blockHash_;
        nIndex = posInBlock;
    };

    bool IsAbandoned() const { return (blockHash == ABANDON_HASH); }
    bool HashUnset() const { return (blockHash.IsNull() || blockHash == ABANDON_HASH); }

    void SetAbandoned()
    {
        blockHash = ABANDON_HASH;
    };

    int64_t GetTxTime() const
    {
        if (HashUnset() || nIndex < 0)
            return nTimeReceived;
        return std::min(nTimeReceived, nBlockTime);
    };

    bool HaveChange() const
    {
        for (const auto &r : vout)
            if (r.nFlags & ORF_CHANGE)
                return true;
        return false;
    };

    CAmount TotalOutput()
    {
        CAmount nTotal = 0;
        for (auto &r : vout)
            nTotal += r.nValue;
        return nTotal;
    };

    mutable uint32_t nCacheFlags;

    bool InMempool() const;
    bool IsTrusted() const;

    bool IsCoinBase() const {return false;}
    bool IsCoinStake() const {return false;}


    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action)
    {
        READWRITE(blockHash);
        READWRITE(nFlags);
        READWRITE(nIndex);
        READWRITE(nBlockTime);
        READWRITE(nTimeReceived);
        READWRITE(mapValue);
        READWRITE(nFee);
        READWRITE(vin);
        READWRITE(vout);
    };
};

typedef std::map<uint256, CTransactionRecord> MapRecords_t;
typedef std::multimap<int64_t, std::map<uint256, CTransactionRecord>::iterator> RtxOrdered_t;

/** (client) version numbers for particular wallet features */
enum WalletFeature
{
    FEATURE_BASE = 10500, // the earliest version new wallets supports (only useful for getwalletinfo's clientversion output)

    FEATURE_WALLETCRYPT = 40000, // wallet encryption
    FEATURE_COMPRPUBKEY = 60000, // compressed public keys

    FEATURE_HD = 130000, // Hierarchical key derivation after BIP32 (HD Wallet)

    FEATURE_HD_SPLIT = 139900, // Wallet with HD chain split (change outputs will use m/0'/1'/k)

    FEATURE_NO_DEFAULT_KEY = 159900, // Wallet without a default key written

    FEATURE_PRE_SPLIT_KEYPOOL = 169900, // Upgraded to HD SPLIT and can have a pre-split keypool

    FEATURE_LATEST = FEATURE_PRE_SPLIT_KEYPOOL
};

//! Default for -addresstype
constexpr OutputType DEFAULT_ADDRESS_TYPE{OutputType::P2SH_SEGWIT};

//! Default for -changetype
constexpr OutputType DEFAULT_CHANGE_TYPE{OutputType::CHANGE_AUTO};

enum WalletFlags : uint64_t {
    // wallet flags in the upper section (> 1 << 31) will lead to not opening the wallet if flag is unknown
    // unknown wallet flags in the lower section <= (1 << 31) will be tolerated

    // will enforce the rule that the wallet can't contain any private keys (only watch-only/pubkeys)
    WALLET_FLAG_DISABLE_PRIVATE_KEYS = (1ULL << 32),
};

static constexpr uint64_t g_known_wallet_flags = WALLET_FLAG_DISABLE_PRIVATE_KEYS;

/** A key pool entry */
class CKeyPool
{
public:
    int64_t nTime;
    CPubKey vchPubKey;
    bool fInternal; // for change outputs
    bool m_pre_split; // For keys generated before keypool split upgrade

    CKeyPool();
    CKeyPool(const CPubKey& vchPubKeyIn, bool internalIn);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(nTime);
        READWRITE(vchPubKey);
        if (ser_action.ForRead()) {
            try {
                READWRITE(fInternal);
            }
            catch (std::ios_base::failure&) {
                /* flag as external address if we can't read the internal boolean
                   (this will be the case for any wallet before the HD chain split version) */
                fInternal = false;
            }
            try {
                READWRITE(m_pre_split);
            }
            catch (std::ios_base::failure&) {
                /* flag as postsplit address if we can't read the m_pre_split boolean
                   (this will be the case for any wallet that upgrades to HD chain split)*/
                m_pre_split = false;
            }
        }
        else {
            READWRITE(fInternal);
            READWRITE(m_pre_split);
        }
    }
};

class CZerocoinEntry
{
private:
    template <typename Stream>
    auto is_eof_helper(Stream &s, bool) -> decltype(s.eof()) {
        return s.eof();
    }

    template <typename Stream>
    bool is_eof_helper(Stream &s, int) {
        return false;
    }

    template<typename Stream>
    bool is_eof(Stream &s) {
        return is_eof_helper(s, true);
    }

public:
    //public
    Bignum value;
    int denomination;
    //private
    Bignum randomness;
    Bignum serialNumber;
    vector<unsigned char> ecdsaSecretKey;

    bool IsUsed;
    int nHeight;
    int id;

    CZerocoinEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        IsUsed = false;
        randomness = 0;
        serialNumber = 0;
        value = 0;
        denomination = -1;
        nHeight = -1;
        id = -1;
    }

    bool IsCorrectV2Mint() const {
        return value > 0 && randomness > 0 && serialNumber > 0 && serialNumber.bitSize() <= 160 &&
                ecdsaSecretKey.size() >= 32;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(IsUsed);
        READWRITE(randomness);
        READWRITE(serialNumber);
        READWRITE(value);
        READWRITE(denomination);
        READWRITE(nHeight);
        READWRITE(id);
        if (ser_action.ForRead()) {
            if (!is_eof(s)) {
                int nStoredVersion = 0;
                READWRITE(nStoredVersion);
                READWRITE(ecdsaSecretKey);
            }
        }
        else {
            int nVer = ZEROCOIN_VERSION_1;
            READWRITE(nVer);
            READWRITE(ecdsaSecretKey);
        }
    }

};

class CZerocoinSpendEntry
{
public:
    Bignum coinSerial;
    uint256 hashTx;
    Bignum pubCoin;
    int denomination;
    int id;

    CZerocoinSpendEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        coinSerial = 0;
        pubCoin = 0;
        denomination = 0;
        id = 0;
    }
    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(coinSerial);
        READWRITE(hashTx);
        READWRITE(pubCoin);
        READWRITE(denomination);
        READWRITE(id);
    }
};

bool CompHeight(const CZerocoinEntry & a, const CZerocoinEntry & b);
bool CompID(const CZerocoinEntry & a, const CZerocoinEntry & b);

/** Address book data */
class CAddressBookData
{
public:
    std::string name;
    std::string purpose = "unknown";
    bool fBech32 = false;

    std::vector<uint32_t> vPath; // index to m is stored in first entry

    mutable uint8_t nOwned = 0; // 0 unknown, 1 yes, 2 no

    ADD_SERIALIZE_METHODS;
    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action)
    {
        READWRITE(name);
        READWRITE(purpose);
        READWRITE(vPath);
        READWRITE(destdata);

        try { READWRITE(fBech32); } catch(std::exception &e) {
            // old format
        }
    }

    typedef std::map<std::string, std::string> StringMap;
    StringMap destdata;
};

struct CRecipient
{
    CScript scriptPubKey;
    CAmount nAmount;
    bool fSubtractFeeFromAmount;
};

class CTempRecipient
{
public:
    CTempRecipient() : nType(0), nAmount(0), nAmountSelected(0), fSubtractFeeFromAmount(false) {SetNull();}
    CTempRecipient(CAmount nAmount_, bool fSubtractFeeFromAmount_, CScript scriptPubKey_)
        : nAmount(nAmount_), nAmountSelected(nAmount_), fSubtractFeeFromAmount(fSubtractFeeFromAmount_), scriptPubKey(scriptPubKey_) {SetNull();}

    void SetNull()
    {
        fNonceSet = false; // if true use nonce and vData from CTempRecipient
        fScriptSet = false;
        fChange = false;
        nChildKey = 0;
        nChildKeyColdStaking = 0;
        nStealthPrefix = 0;
        fExemptFeeSub = false;
    };

    void SetAmount(CAmount nValue)
    {
        nAmount = nValue;
        nAmountSelected = nValue;
    };

    bool ApplySubFee(CAmount nFee, size_t nSubtractFeeFromAmount, bool &fFirst);

    uint8_t nType;
    CAmount nAmount;            // If fSubtractFeeFromAmount, nAmount = nAmountSelected - feeForOutput
    CAmount nAmountSelected;
    bool fSubtractFeeFromAmount;
    bool fExemptFeeSub;         // Value too low to sub fee when blinded value split into two outputs
    CTxDestination address;
    CTxDestination addressColdStaking;
    CScript scriptPubKey;
    std::vector<uint8_t> vData;
    uint256 nonce;

    CKey sEphem;
    CPubKey pkTo;
    int n;
    std::string sNarration;
    bool fScriptSet;
    bool fChange;
    bool fNonceSet;
    uint32_t nChildKey; // update later
    uint32_t nChildKeyColdStaking; // update later
    uint32_t nStealthPrefix;
};

typedef std::map<std::string, std::string> mapValue_t;


static inline void ReadOrderPos(int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (!mapValue.count("n"))
    {
        nOrderPos = -1; // TODO: calculate elsewhere
        return;
    }
    nOrderPos = atoi64(mapValue["n"].c_str());
}


static inline void WriteOrderPos(const int64_t& nOrderPos, mapValue_t& mapValue)
{
    if (nOrderPos == -1)
        return;
    mapValue["n"] = i64tostr(nOrderPos);
}

struct COutputEntry
{
    CTxDestination destination;
    CAmount amount;
    int vout;
    isminetype ismine;
    CTxDestination destStake;
};

/** A transaction with a merkle branch linking it to the block chain. */
class CMerkleTx
{
private:
  /** Constant used in hashBlock to indicate tx has been abandoned */
    static const uint256 ABANDON_HASH;

public:
    CTransactionRef tx;
    uint256 hashBlock;

    /* An nIndex == -1 means that hashBlock (in nonzero) refers to the earliest
     * block in the chain we know this or any in-wallet dependency conflicts
     * with. Older clients interpret nIndex == -1 as unconfirmed for backward
     * compatibility.
     */
    int nIndex;
    mutable bool fHeightCached;
    mutable int nCachedHeight;

    CMerkleTx()
    {
        SetTx(MakeTransactionRef());
        Init();
    }

    explicit CMerkleTx(CTransactionRef arg)
    {
        SetTx(std::move(arg));
        Init();
    }

    void Init()
    {
        hashBlock = uint256();
        nIndex = -1;
        fHeightCached = false;
    }

    void SetTx(CTransactionRef arg)
    {
        tx = std::move(arg);
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        std::vector<uint256> vMerkleBranch; // For compatibility with older versions.
        READWRITE(tx);
        READWRITE(hashBlock);
        READWRITE(vMerkleBranch);
        READWRITE(nIndex);
    }

    void SetMerkleBranch(const CBlockIndex* pIndex, int posInBlock);

    /**
     * Return depth of transaction in blockchain:
     * <0  : conflicts with a transaction this deep in the blockchain
     *  0  : in memory pool, waiting to be included in a block
     * >=1 : this many blocks deep in the main chain
     */
    int GetDepthInMainChain() const;
    bool IsInMainChain() const { return GetDepthInMainChain() > 0; }
    int GetBlocksToMaturity() const;
    bool hashUnset() const { return (hashBlock.IsNull() || hashBlock == ABANDON_HASH); }
    bool isAbandoned() const { return (hashBlock == ABANDON_HASH); }
    void setAbandoned() { hashBlock = ABANDON_HASH; }

    const uint256& GetHash() const { return tx->GetHash(); }
    bool IsCoinBase() const { return tx->IsCoinBase(); }
    bool IsCoinStake() const { return tx->IsCoinStake(); }
};

//Get the marginal bytes of spending the specified output
int CalculateMaximumSignedInputSize(const CTxOut& txout, const CWallet* pwallet, bool use_max_sig = false);

/**
 * A transaction with a bunch of additional info that only the owner cares about.
 * It includes any unrecorded transactions needed to link it back to the block chain.
 */
class CWalletTx : public CMerkleTx
{
private:
    const CWallet* pwallet;

public:
    /**
     * Key/value map with information about the transaction.
     *
     * The following keys can be read and written through the map and are
     * serialized in the wallet database:
     *
     *     "comment", "to"   - comment strings provided to sendtoaddress,
     *                         sendfrom, sendmany wallet RPCs
     *     "replaces_txid"   - txid (as HexStr) of transaction replaced by
     *                         bumpfee on transaction created by bumpfee
     *     "replaced_by_txid" - txid (as HexStr) of transaction created by
     *                         bumpfee on transaction replaced by bumpfee
     *     "from", "message" - obsolete fields that could be set in UI prior to
     *                         2011 (removed in commit 4d9b223)
     *
     * The following keys are serialized in the wallet database, but shouldn't
     * be read or written through the map (they will be temporarily added and
     * removed from the map during serialization):
     *
     *     "fromaccount"     - serialized strFromAccount value
     *     "n"               - serialized nOrderPos value
     *     "timesmart"       - serialized nTimeSmart value
     *     "spent"           - serialized vfSpent value that existed prior to
     *                         2014 (removed in commit 93a18a3)
     */
    mapValue_t mapValue;
    std::vector<std::pair<std::string, std::string> > vOrderForm;
    unsigned int fTimeReceivedIsTxTime;
    unsigned int nTimeReceived; //!< time received by this node
    /**
     * Stable timestamp that never changes, and reflects the order a transaction
     * was added to the wallet. Timestamp is based on the block time for a
     * transaction added as part of a block, or else the time when the
     * transaction was received if it wasn't part of a block, with the timestamp
     * adjusted in both cases so timestamp order matches the order transactions
     * were added to the wallet. More details can be found in
     * CWallet::ComputeTimeSmart().
     */
    unsigned int nTimeSmart;
    /**
     * From me flag is set to 1 for transactions that were created by the wallet
     * on this bitcoin node, and set to 0 for transactions that were created
     * externally and came in through the network or sendrawtransaction RPC.
     */
    char fFromMe;
    std::string strFromAccount;
    int64_t nOrderPos; //!< position in ordered transaction list
    std::multimap<int64_t, std::pair<CWalletTx*, CAccountingEntry*>>::const_iterator m_it_wtxOrdered;

    // memory only
    mutable bool fDebitCached;
    mutable bool fCreditCached;
    mutable bool fImmatureCreditCached;
    mutable bool fAvailableCreditCached;
    mutable bool fWatchDebitCached;
    mutable bool fWatchCreditCached;
    mutable bool fImmatureWatchCreditCached;
    mutable bool fAvailableWatchCreditCached;
    mutable bool fChangeCached;
    mutable bool fInMempool;
    mutable CAmount nDebitCached;
    mutable CAmount nCreditCached;
    mutable CAmount nImmatureCreditCached;
    mutable CAmount nAvailableCreditCached;
    mutable CAmount nWatchDebitCached;
    mutable CAmount nWatchCreditCached;
    mutable CAmount nImmatureWatchCreditCached;
    mutable CAmount nAvailableWatchCreditCached;
    mutable CAmount nChangeCached;

    CWalletTx()
    {
        Init(nullptr);
    }

    CWalletTx(const CWallet* pwalletIn, CTransactionRef arg) : CMerkleTx(std::move(arg))
    {
        Init(pwalletIn);
    }

    void Init(const CWallet* pwalletIn)
    {
        pwallet = pwalletIn;
        mapValue.clear();
        vOrderForm.clear();
        fTimeReceivedIsTxTime = false;
        nTimeReceived = 0;
        nTimeSmart = 0;
        fFromMe = false;
        strFromAccount.clear();
        fDebitCached = false;
        fCreditCached = false;
        fImmatureCreditCached = false;
        fAvailableCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fChangeCached = false;
        fInMempool = false;
        nDebitCached = 0;
        nCreditCached = 0;
        nImmatureCreditCached = 0;
        nAvailableCreditCached = 0;
        nWatchDebitCached = 0;
        nWatchCreditCached = 0;
        nAvailableWatchCreditCached = 0;
        nImmatureWatchCreditCached = 0;
        nChangeCached = 0;
        nOrderPos = -1;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        if (ser_action.ForRead())
            Init(nullptr);
        char fSpent = false;

        if (!ser_action.ForRead())
        {
            mapValue["fromaccount"] = strFromAccount;

        s << static_cast<const CMerkleTx&>(*this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        s << vUnused << mapValueCopy << vOrderForm << fTimeReceivedIsTxTime << nTimeReceived << fFromMe << fSpent;
    }

            if (nTimeSmart)
                mapValue["timesmart"] = strprintf("%u", nTimeSmart);
        }

        s >> static_cast<CMerkleTx&>(*this);
        std::vector<CMerkleTx> vUnused; //!< Used to be vtxPrev
        READWRITE(vUnused);
        READWRITE(mapValue);
        READWRITE(vOrderForm);
        READWRITE(fTimeReceivedIsTxTime);
        READWRITE(nTimeReceived);
        READWRITE(fFromMe);
        READWRITE(fSpent);

        if (ser_action.ForRead())
        {
            strFromAccount = mapValue["fromaccount"];

            ReadOrderPos(nOrderPos, mapValue);

            nTimeSmart = mapValue.count("timesmart") ? (unsigned int)atoi64(mapValue["timesmart"]) : 0;
        }

        mapValue.erase("fromaccount");
        mapValue.erase("spent");
        mapValue.erase("n");
        mapValue.erase("timesmart");
    }

    //! make sure balances are recalculated
    void MarkDirty()
    {
        fCreditCached = false;
        fAvailableCreditCached = false;
        fImmatureCreditCached = false;
        fWatchDebitCached = false;
        fWatchCreditCached = false;
        fAvailableWatchCreditCached = false;
        fImmatureWatchCreditCached = false;
        fDebitCached = false;
        fChangeCached = false;
    }

    void BindWallet(CWallet *pwalletIn)
    {
        pwallet = pwalletIn;
        MarkDirty();
    }

    //! filter decides which addresses will count towards the debit
    CAmount GetDebit(const isminefilter& filter) const;
    CAmount GetCredit(const isminefilter& filter) const;
    CAmount GetImmatureCredit(bool fUseCache=true) const;
    CAmount GetAvailableCredit(bool fUseCache=true, const isminefilter& filter=ISMINE_SPENDABLE, const bool fForStaking=false) const;
    CAmount GetImmatureWatchOnlyCredit(const bool fUseCache=true) const;
    CAmount GetChange() const;
    CAmount GetAnonymizedCredit(bool fUseCache=true) const;

    // Get the marginal bytes if spending the specified output from this transaction
    int GetSpendSize(unsigned int out, bool use_max_sig = false) const
    {
        return CalculateMaximumSignedInputSize(tx->vout[out], pwallet, use_max_sig);
    }

    void GetAmounts(std::list<COutputEntry>& listReceived,
                    std::list<COutputEntry>& listSent, std::list<COutputEntry> &listStaked, CAmount& nFee, std::string& strSentAccount, const isminefilter& filter) const;

    bool IsFromMe(const isminefilter& filter) const
    {
        return (GetDebit(filter) > 0);
    }

    // True if only scriptSigs are different
    bool IsEquivalentTo(const CWalletTx& tx) const;

    bool InMempool() const;
    bool IsTrusted() const;

    int64_t GetTxTime() const;

    // RelayWalletTransaction may only be called if fBroadcastTransactions!
    bool RelayWalletTransaction(CConnman* connman);

    /** Pass this transaction to the mempool. Fails if absolute fee exceeds absurd fee. */
    bool AcceptToMemoryPool(const CAmount& nAbsurdFee, CValidationState& state);

    std::set<uint256> GetConflicts() const;
};


class CInputCoin {
public:
    CInputCoin(const CWalletTx* walletTx, unsigned int i)
    {
        if (!walletTx)
            throw std::invalid_argument("walletTx should not be null");
        if (i >= walletTx->tx->vout.size())
            throw std::out_of_range("The output index is out of range");

        outpoint = COutPoint(walletTx->GetHash(), i);
        txout = walletTx->tx->vout[i];
        walletTX = walletTx;
        index = i;
    }

    const CWalletTx *walletTX;
    unsigned int index;
    COutPoint outpoint;
    CTxOut txout;

    bool operator<(const CInputCoin& rhs) const {
        return outpoint < rhs.outpoint;
    }

    bool operator!=(const CInputCoin& rhs) const {
        return outpoint != rhs.outpoint;
    }

    bool operator==(const CInputCoin& rhs) const {
        return outpoint == rhs.outpoint;
    }
};

class COutput
{
public:
    const CWalletTx *tx;
    int i;
    int nDepth;

    /** Whether we have the private keys to spend this output */
    bool fSpendable;

    /** Whether we know how to spend this output, ignoring the lack of keys */
    bool fSolvable;

    /** Whether to use the maximum sized, 72 byte signature when calculating the size of the input spend. This should only be set when watch-only outputs are allowed */
    bool use_max_sig;

    /**
     * Whether this output is considered safe to spend. Unconfirmed transactions
     * from outside keys and unconfirmed replacement transactions are considered
     * unsafe and will not be used to fund new spending transactions.
     */
    bool fSafe;

    COutput(const CWalletTx *txIn, int iIn, int nDepthIn, bool fSpendableIn, bool fSolvableIn, bool fSafeIn, bool use_max_sig_in = false)
    {
        tx = txIn; i = iIn; nDepth = nDepthIn; fSpendable = fSpendableIn; fSolvable = fSolvableIn; fSafe = fSafeIn; nInputBytes = -1; use_max_sig = use_max_sig_in;
        // If known and signable by the given wallet, compute nInputBytes
        // Failure will keep this value -1
        if (fSpendable && tx) {
            nInputBytes = tx->GetSpendSize(i, use_max_sig);
        }
    }

    int Priority() const;
    std::string ToString() const;

    inline CInputCoin GetInputCoin() const
    {
        return CInputCoin(tx->tx, i, nInputBytes);
    }
};

/** Private key that includes an expiration date in case it never gets used. */
class CWalletKey
{
public:
    CPrivKey vchPrivKey;
    int64_t nTimeCreated;
    int64_t nTimeExpires;
    std::string strComment;
    //! todo: add something to note what created it (user, getnewaddress, change)
    //!   maybe should have a map<string, string> property map

    explicit CWalletKey(int64_t nExpires=0);

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPrivKey);
        READWRITE(nTimeCreated);
        READWRITE(nTimeExpires);
        READWRITE(LIMITED_STRING(strComment, 65536));
    }
};

/**
 * DEPRECATED Internal transfers.
 * Database key is acentry<account><counter>.
 */
class CAccountingEntry
{
public:
    std::string strAccount;
    CAmount nCreditDebit;
    int64_t nTime;
    std::string strOtherAccount;
    std::string strComment;
    mapValue_t mapValue;
    int64_t nOrderPos; //!< position in ordered transaction list
    uint64_t nEntryNo;

    CAccountingEntry()
    {
        SetNull();
    }

    void SetNull()
    {
        nCreditDebit = 0;
        nTime = 0;
        strAccount.clear();
        strOtherAccount.clear();
        strComment.clear();
        nOrderPos = -1;
        nEntryNo = 0;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        //! Note: strAccount is serialized as part of the key, not here.
        READWRITE(nCreditDebit);
        READWRITE(nTime);
        READWRITE(LIMITED_STRING(strOtherAccount, 65536));

        if (!ser_action.ForRead())
        {
            WriteOrderPos(nOrderPos, mapValue);

            if (!(mapValue.empty() && _ssExtra.empty()))
            {
                CDataStream ss(s.GetType(), s.GetVersion());
                ss.insert(ss.begin(), '\0');
                ss << mapValue;
                ss.insert(ss.end(), _ssExtra.begin(), _ssExtra.end());
                strComment.append(ss.str());
            }
        }

        READWRITE(LIMITED_STRING(strComment, 65536));

        size_t nSepPos = strComment.find("\0", 0, 1);
        if (ser_action.ForRead())
        {
            mapValue.clear();
            if (std::string::npos != nSepPos)
            {
                CDataStream ss(std::vector<char>(strComment.begin() + nSepPos + 1, strComment.end()), s.GetType(), s.GetVersion());
                ss >> mapValue;
                _ssExtra = std::vector<char>(ss.begin(), ss.end());
            }
            ReadOrderPos(nOrderPos, mapValue);
        }
        if (std::string::npos != nSepPos)
            strComment.erase(nSepPos);

        mapValue.erase("n");
    }

private:
    std::vector<char> _ssExtra;
};

struct CoinSelectionParams
{
    bool use_bnb = true;
    size_t change_output_size = 0;
    size_t change_spend_size = 0;
    CFeeRate effective_fee = CFeeRate(0);
    size_t tx_noinputs_size = 0;

    CoinSelectionParams(bool use_bnb, size_t change_output_size, size_t change_spend_size, CFeeRate effective_fee, size_t tx_noinputs_size) : use_bnb(use_bnb), change_output_size(change_output_size), change_spend_size(change_spend_size), effective_fee(effective_fee), tx_noinputs_size(tx_noinputs_size) {}
    CoinSelectionParams() {}
};

class WalletRescanReserver; //forward declarations for ScanForWalletTransactions/RescanFromTime
/**
 * A CWallet is an extension of a keystore, which also maintains a set of transactions and balances,
 * and provides the ability to create new transactions.
 */
class CWallet final : public CCryptoKeyStore, public CValidationInterface
{
private:
    std::atomic<bool> fAbortRescan{false};
    std::atomic<bool> fScanningWallet{false}; // controlled by WalletRescanReserver
    std::mutex mutexScanning;
    friend class WalletRescanReserver;

    bool SelectCoins(const std::vector<COutput>& vAvailableCoins, const CAmount& nTargetValue, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet, const CCoinControl *coinControl = nullptr, AvailableCoinsType nCoinType = ALL_COINS, bool fUseInstantSend = false) const;

    WalletBatch *encrypted_batch = nullptr;

    //! the current wallet version: clients below this version are not able to load the wallet
    int nWalletVersion = FEATURE_BASE;

    //! the maximum wallet format version: memory-only variable that specifies to what version this wallet may be upgraded
    int nWalletMaxVersion = FEATURE_BASE;

    int64_t nNextResend = 0;
    int64_t nLastResend = 0;
    bool fBroadcastTransactions = false;

    mutable bool fAnonymizableTallyCached;
    mutable std::vector<CompactTallyItem> vecAnonymizableTallyCached;
    mutable bool fAnonymizableTallyCachedNonDenom;
    mutable std::vector<CompactTallyItem> vecAnonymizableTallyCachedNonDenom;

    /**
     * Used to keep track of spent outpoints, and
     * detect and report conflicts (double-spends or
     * mutated transactions where the mutant gets mined).
     */
    typedef std::multimap<COutPoint, uint256> TxSpends;
    TxSpends mapTxSpends;
    void AddToSpends(const COutPoint& outpoint, const uint256& wtxid);
    void AddToSpends(const uint256& wtxid);

    /**
     * Add a transaction to the wallet, or update it.  pIndex and posInBlock should
     * be set when the transaction was known to be included in a block.  When
     * pIndex == nullptr, then wallet state is not updated in AddToWallet, but
     * notifications happen and cached balances are marked dirty.
     *
     * If fUpdate is true, existing transactions will be updated.
     * TODO: One exception to this is that the abandoned state is cleared under the
     * assumption that any further notification of a transaction that was considered
     * abandoned is an indication that it is not safe to be considered abandoned.
     * Abandoned state should probably be more carefully tracked via different
     * posInBlock signals or by checking mempool presence when necessary.
     */
    bool AddToWalletIfInvolvingMe(const CTransactionRef& tx, const CBlockIndex* pIndex, int posInBlock, bool fUpdate) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /* Mark a transaction (and its in-wallet descendants) as conflicting with a particular block. */
    void MarkConflicted(const uint256& hashBlock, const uint256& hashTx);

    /* Mark a transaction's inputs dirty, thus forcing the outputs to be recomputed */
    void MarkInputsDirty(const CTransactionRef& tx);

    void SyncMetaData(std::pair<TxSpends::iterator, TxSpends::iterator>);

    /* Used by TransactionAddedToMemorypool/BlockConnected/Disconnected/ScanForWalletTransactions.
     * Should be called with pindexBlock and posInBlock if this is for a transaction that is included in a block. */
    void SyncTransaction(const CTransactionRef& tx, const CBlockIndex *pindex = nullptr, int posInBlock = 0, bool update_tx = true) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /* the HD chain data model (external chain counters) */
    CHDChain hdChain;

    /* HD derive new child key (on internal or external chain) */
    void DeriveNewChildKey(WalletBatch &batch, CKeyMetadata& metadata, CKey& secret, bool internal = false) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    std::set<int64_t> setInternalKeyPool;
    std::set<int64_t> setExternalKeyPool;
    std::set<int64_t> set_pre_split_keypool;
    int64_t m_max_keypool_index = 0;
    std::map<CKeyID, int64_t> m_pool_key_to_index;
    std::atomic<uint64_t> m_wallet_flags{0};

    int64_t nTimeFirstKey = 0;

    /**
     * Private version of AddWatchOnly method which does not accept a
     * timestamp, and which will reset the wallet's nTimeFirstKey value to 1 if
     * the watch key did not previously have a timestamp associated with it.
     * Because this is an inherited virtual method, it is accessible despite
     * being marked private, but it is marked private anyway to encourage use
     * of the other AddWatchOnly which accepts a timestamp and sets
     * nTimeFirstKey more intelligently for more efficient rescans.
     */
    bool AddWatchOnly(const CScript& dest) override EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Wallet filename from wallet=<path> command line or config option.
     * Used in debug logs and to send RPCs to the right wallet instance when
     * more than one wallet is loaded.
     */
    std::string m_name;

    /** Internal database handle. */
    std::unique_ptr<WalletDatabase> database;

    /**
     * The following is used to keep track of how far behind the wallet is
     * from the chain sync, and to allow clients to block on us being caught up.
     *
     * Note that this is *not* how far we've processed, we may need some rescan
     * to have seen all transactions in the chain, but is only used to track
     * live BlockConnected callbacks.
     *
     * Protected by cs_main (see BlockUntilSyncedToCurrentChain)
     */
    const CBlockIndex* m_last_block_processed = nullptr;

public:
    /*
     * Main wallet lock.
     * This lock protects all the fields added by CWallet.
     */
    mutable CCriticalSection cs_wallet;

    int walletVersion;

    /** Get database handle used by this wallet. Ideally this function would
     * not be necessary.
     */
    WalletDatabase& GetDBHandle()
    {
        return *database;
    }

    /** Get a name for this wallet for logging/debugging purposes.
     */
    std::string GetName() const
    {
        if (dbw) {
            return dbw->GetName();
        } else {
            return "dummy";
        }
    }

    void LoadKeyPool(int64_t nIndex, const CKeyPool &keypool) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void MarkPreSplitKeys();

    // Map from Key ID to key metadata.
    std::map<CKeyID, CKeyMetadata> mapKeyMetadata;
    //Ghostnode
    int64_t nKeysLeftSinceAutoBackup;

    // Map from Script ID to key metadata (for watch-only keys).
    std::map<CScriptID, CKeyMetadata> m_script_metadata;

    typedef std::map<unsigned int, CMasterKey> MasterKeyMap;
    MasterKeyMap mapMasterKeys;
    unsigned int nMasterKeyMaxID = 0;

    /** Construct wallet with specified name and database implementation. */
    CWallet(std::string name, std::unique_ptr<WalletDatabase> database) : m_name(std::move(name)), database(std::move(database))
    {
    }

    ~CWallet()
    {
        delete encrypted_batch;
        encrypted_batch = nullptr;
    }

    std::map<uint256, CWalletTx> mapWallet;
    std::list<CAccountingEntry> laccentries;

    typedef std::pair<CWalletTx*, CAccountingEntry*> TxPair;
    typedef std::multimap<int64_t, TxPair > TxItems;
    TxItems wtxOrdered;

    int64_t nOrderPosNext = 0;
    uint64_t nAccountingEntryNumber = 0;

    std::map<CTxDestination, CAddressBookData> mapAddressBook;

    std::set<COutPoint> setLockedCoins;

    const CWalletTx* GetWalletTx(const uint256& hash) const;

    //! check whether we are allowed to upgrade (or already support) to the named feature
    bool CanSupportFeature(enum WalletFeature wf) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) { AssertLockHeld(cs_wallet); return nWalletMaxVersion >= wf; }

    /**
     * populate vCoins with vector of available COutputs.
     */
    void AvailableCoins(std::vector<COutput>& vCoins, bool fOnlySafe=true, const CCoinControl *coinControl = nullptr, const CAmount& nMinimumAmount = 1, const CAmount& nMaximumAmount = MAX_MONEY, const CAmount& nMinimumSumAmount = MAX_MONEY, const uint64_t nMaximumCount = 0, const int nMinDepth = 0, const int nMaxDepth = 9999999, AvailableCoinsType nCoinType=ALL_COINS, bool includeImmature = false) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Return list of available coins and locked coins grouped by non-change output address.
     */
    std::map<CTxDestination, std::vector<COutput>> ListCoins() const;

    /**
     * Find non-change parent output.
     */
    const CTxOut& FindNonChangeParentOutput(const CTransaction& tx, int output) const;

    /**
     * Shuffle and select coins until nTargetValue is reached while avoiding
     * small change; This method is stochastic for some inputs and upon
     * completion the coin set and corresponding actual target value is
     * assembled
     */
    bool SelectCoinsMinConf(const CAmount& nTargetValue, int nConfMine, int nConfTheirs, uint64_t nMaxAncestors, std::vector<COutput> vCoins, std::set<CInputCoin>& setCoinsRet, CAmount& nValueRet) const;

    bool SelectCoinsByDenominations(int nDenom, CAmount nValueMin, CAmount nValueMax, std::vector<CTxIn>& vecTxInRet, std::vector<COutput>& vCoinsRet, CAmount& nValueRet, int nPrivateSendRoundsMin, int nPrivateSendRoundsMax);
    bool GetCollateralTxIn(CTxIn& txinRet, CAmount& nValueRet) const;
    bool SelectCoinsDark(CAmount nValueMin, CAmount nValueMax, std::vector<CTxIn>& vecTxInRet, CAmount& nValueRet, int nPrivateSendRoundsMin, int nPrivateSendRoundsMax) const;
    bool SelectCoinsGrouppedByAddresses(std::vector<CompactTallyItem>& vecTallyRet, bool fSkipDenominated = true, bool fAnonymizable = true) const;

    bool IsSpent(const uint256& hash, unsigned int n) const;
    std::vector<OutputGroup> GroupOutputs(const std::vector<COutput>& outputs, bool single_coin) const;

    bool IsLockedCoin(uint256 hash, unsigned int n) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void LockCoin(const COutPoint& output) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void UnlockCoin(const COutPoint& output) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void UnlockAllCoins() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void ListLockedCoins(std::vector<COutPoint>& vOutpts) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    // ghostnode
    /// Get 40000 NIX output and keys which can be used for the Ghostnode
    bool GetGhostnodeVinAndKeys(CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet, std::string strTxHash = "", std::string strOutputIndex = "");
    /// Extract txin information and keys from output
    bool GetVinAndKeysFromOutput(COutput out, CTxIn& txinRet, CPubKey& pubKeyRet, CKey& keyRet);
    bool HasCollateralInputs(bool fOnlyConfirmed = true) const;
    int  CountInputsWithAmount(CAmount nInputAmount);

    /*
     * Rescan abort properties
     */
    void AbortRescan() { fAbortRescan = true; }
    bool IsAbortingRescan() { return fAbortRescan; }
    bool IsScanning() { return fScanningWallet; }

    /**
     * keystore implementation
     * Generate a new key
     */
    CPubKey GenerateNewKey(WalletBatch& batch, bool internal = false) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    //! Adds a key to the store, and saves it to disk.
    bool AddKeyPubKey(const CKey& key, const CPubKey &pubkey) override EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool AddKeyPubKeyWithDB(WalletBatch &batch,const CKey& key, const CPubKey &pubkey) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    //! Adds a key to the store, without saving it to disk (used by LoadWallet)
    bool LoadKey(const CKey& key, const CPubKey &pubkey) { return CCryptoKeyStore::AddKeyPubKey(key, pubkey); }
    //! Load metadata (used by LoadWallet)
    void LoadKeyMetadata(const CKeyID& keyID, const CKeyMetadata &metadata) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    void LoadScriptMetadata(const CScriptID& script_id, const CKeyMetadata &metadata) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool LoadMinVersion(int nVersion) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet) { AssertLockHeld(cs_wallet); nWalletVersion = nVersion; nWalletMaxVersion = std::max(nWalletMaxVersion, nVersion); return true; }
    void UpdateTimeFirstKey(int64_t nCreateTime) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    //! Adds an encrypted key to the store, and saves it to disk.
    bool AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret) override;
    //! Adds an encrypted key to the store, without saving it to disk (used by LoadWallet)
    bool LoadCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret);
    bool AddCScript(const CScript& redeemScript) override;
    bool LoadCScript(const CScript& redeemScript);

    //! Adds a destination data tuple to the store, and saves it to disk
    bool AddDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Erases a destination data tuple in the store and on disk
    bool EraseDestData(const CTxDestination &dest, const std::string &key);
    //! Adds a destination data tuple to the store, without saving it to disk
    void LoadDestData(const CTxDestination &dest, const std::string &key, const std::string &value);
    //! Look up a destination data tuple in the store, return true if found false otherwise
    bool GetDestData(const CTxDestination &dest, const std::string &key, std::string *value) const;
    //! Get all destination values matching a prefix.
    std::vector<std::string> GetDestValues(const std::string& prefix) const;

    //! Adds a watch-only address to the store, and saves it to disk.
    bool AddWatchOnly(const CScript& dest, int64_t nCreateTime) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool RemoveWatchOnly(const CScript &dest) override EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    //! Adds a watch-only address to the store, without saving it to disk (used by LoadWallet)
    bool LoadWatchOnly(const CScript &dest);

    //! Holds a timestamp at which point the wallet is scheduled (externally) to be relocked. Caller must arrange for actual relocking to occur via Lock().
    int64_t nRelockTime = 0;

    bool Unlock(const SecureString& strWalletPassphrase);
    bool ChangeWalletPassphrase(const SecureString& strOldWalletPassphrase, const SecureString& strNewWalletPassphrase);
    bool EncryptWallet(const SecureString& strWalletPassphrase);

    void GetKeyBirthTimes(std::map<CTxDestination, int64_t> &mapKeyBirth) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    unsigned int ComputeTimeSmart(const CWalletTx& wtx) const;

    /**
     * Increment the next transaction order id
     * @return next transaction order id
     */
    int64_t IncOrderPosNext(WalletBatch *batch = nullptr) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    DBErrors ReorderTransactions();
    bool AccountMove(std::string strFrom, std::string strTo, CAmount nAmount, std::string strComment = "") EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool GetLabelDestination(CTxDestination &dest, const std::string& label, bool bForceNew = false);

    void MarkDirty();
    bool AddToWallet(const CWalletTx& wtxIn, bool fFlushOnClose=true);
    void LoadToWallet(const CWalletTx& wtxIn);
    void TransactionAddedToMempool(const CTransactionRef& tx) override;
    void BlockConnected(const std::shared_ptr<const CBlock>& pblock, const CBlockIndex *pindex, const std::vector<CTransactionRef>& vtxConflicted) override;
    void BlockDisconnected(const std::shared_ptr<const CBlock>& pblock) override;
    int64_t RescanFromTime(int64_t startTime, const WalletRescanReserver& reserver, bool update);
    CBlockIndex* ScanForWalletTransactions(CBlockIndex* pindexStart, CBlockIndex* pindexStop, const WalletRescanReserver& reserver, bool fUpdate = false);
    void TransactionRemovedFromMempool(const CTransactionRef &ptx) override;
    void ReacceptWalletTransactions();
    void ResendWalletTransactions(int64_t nBestBlockTime, CConnman* connman) override;
    // ResendWalletTransactionsBefore may only be called if fBroadcastTransactions!
    std::vector<uint256> ResendWalletTransactionsBefore(int64_t nTime, CConnman* connman);
    CAmount GetBalance(const isminefilter& filter=ISMINE_SPENDABLE, const int min_depth=0) const;
    CAmount GetStakeableBalance() const;
    CAmount GetUnconfirmedBalance() const;
    CAmount GetImmatureBalance() const;
    CAmount GetWatchOnlyBalance() const;
    CAmount GetGhostBalance() const;
    CAmount GetGhostBalanceUnconfirmed() const;
    CAmount GetUnconfirmedWatchOnlyBalance() const;
    CAmount GetImmatureWatchOnlyBalance() const;
    CAmount GetLegacyBalance(const isminefilter& filter, int minDepth, const std::string* account) const;
    CAmount GetAvailableBalance(const CCoinControl* coinControl = nullptr) const;


    int GetRealInputPrivateSendRounds(CTxIn txin, int nRounds) const;
    // respect current settings
    int GetInputPrivateSendRounds(CTxIn txin) const;
    bool IsDenominated(const CTxIn &txin) const;
    bool IsDenominatedAmount(CAmount nInputAmount) const;
    bool IsCollateralAmount(CAmount nInputAmount) const;
    CAmount GetAnonymizableBalance(bool fSkipDenominated = false) const;
    CAmount GetAnonymizedBalance() const;
    CAmount GetNeedsToBeAnonymizedBalance(CAmount nMinBalance = 0) const;
    CAmount GetDenominatedBalance(bool unconfirmed=false) const;

    //OutputType TransactionChangeType(OutputType change_type, const std::vector<CRecipient>& vecSend);

    /**
     * Insert additional inputs into the transaction by
     * calling CreateTransaction();
     */
    bool FundTransaction(CMutableTransaction& tx, CAmount& nFeeRet, int& nChangePosInOut, std::string& strFailReason, bool lockUnspents, const std::set<int>& setSubtractFeeFromOutputs, CCoinControl);
    bool SignTransaction(CMutableTransaction& tx) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    /**
     * Add zerocoin Mint and Spend function
     */
    void ListAvailableCoinsMintCoins(std::vector<COutput>& vCoins, bool fOnlyConfirmed=true) const;
    bool CreateZerocoinMintTransaction(const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosInOut,
                                       std::string& strFailReason, const CCoinControl &coinControl, bool sign = true);
    bool CreateZerocoinMintTransaction(CScript pubCoin, int64_t nValue,
                                       CWalletTx& wtxNew, CReserveKey& reservekey, int64_t& nFeeRet, std::string& strFailReason, const CCoinControl &coinControl);
    bool CreateZerocoinSpendTransaction(std::string &toKey,int64_t nValue, libzerocoin::CoinDenomination denomination,
                                        CWalletTx& wtxNew, CReserveKey& reservekey, CBigNum& coinSerial, uint256& txHash, CBigNum& zcSelectedValue, bool& zcSelectedIsUsed,  std::string& strFailReason);
    bool CommitZerocoinSpendTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, CConnman* connman, CValidationState& state);
    std::string SendMoney(CScript scriptPubKey, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);
    std::string SendMoneyToDestination(const CTxDestination &address, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);
    std::string MintZerocoin(CScript pubCoin, int64_t nValue, CWalletTx& wtxNew, bool fAskFee=false);
    std::string SpendZerocoin(std::string &toKey, int64_t nValue, libzerocoin::CoinDenomination denomination, CWalletTx& wtxNew, CBigNum& coinSerial, uint256& txHash, CBigNum& zcSelectedValue, bool& zcSelectedIsUsed);
    bool CreateZerocoinMintModel(string &stringError, string denomAmount);
    bool CreateZerocoinSpendModel(string &stringError, string denomAmount, string toAddr="");
    bool SetZerocoinBook(const CZerocoinEntry& zerocoinEntry);

    bool CreateZerocoinMintTransactionBatch(vector <CScript> pubCoinBatch, vector <int64_t> nValueBatch, CWalletTx &wtxNew, CReserveKey &reservekey,
                                           int64_t &nFeeRet, std::string &strFailReason,
                                           const CCoinControl &coinControl);

    std::string MintZerocoinBatch(vector <CScript> pubCoinBatch, vector <int64_t> nValueBatch, CWalletTx &wtxNew, bool fAskFee=false);

    bool CreateZerocoinMintModelBatch(string &stringError, vector <string> denomAmount);
    bool CreateZerocoinMintModelBatch(string &stringError, vector <string> denomAmount, vector<CScript> pubCoinScripts);

    std::string SpendZerocoinBatch(std::string &toKey, vector <CScript> pubCoinScripts, vector <int64_t> nValueBatch, vector <libzerocoin::CoinDenomination> denominationBatch, CWalletTx &wtxNew,
                                   vector <CBigNum> &coinSerialBatch, vector <uint256> &txHashBatch, vector <CBigNum> &zcSelectedValueBatch,
                                   bool &zcSelectedIsUsed);

    bool CreateZerocoinSpendTransactionBatch(std::string &toKey, vector <CScript> pubCoinScripts, vector <int64_t> nValueBatch, vector <libzerocoin::CoinDenomination> denominationBatch,
                                             CWalletTx &wtxNew, CReserveKey &reservekey, vector <CBigNum> &coinSerialBatch,
                                             vector <uint256> &txHashBatch, vector <CBigNum> &zcSelectedValueBatch, bool &zcSelectedIsUsed,
                                             std::string &strFailReason);

    bool CreateZerocoinSpendModelBatch(string &stringError, vector <string> denomAmountBatch, string toAddr, vector <CScript> pubCoinScripts = vector<CScript>());

    bool FindUnloadedGhostTransactions(const CTransaction& tx);
    bool TopUpUnloadedCommitments(int kpSize = 101);
    bool GetKeyPackList(vector <CommitmentKeyPack> &keyPackList, int packSize = 10);

    bool EncryptPrivateZerocoinData(CZerocoinEntry &zerocoinMintPlain);
    bool DecryptPrivateZerocoinData(CZerocoinEntry &zerocoinMintSecret);


    /**
     * Add ghost functions
     */
    bool EnableGhostMode(SecureString strWalletPass,string totalAmount);
    bool DisableGhostMode();
    bool GhostModeMintTrigger(string totalAmount, vector<CScript> pubCoinScripts = vector<CScript>());
    std::string GhostModeSpendTrigger(string denomination, string toKey = "", vector<CScript> pubCoinScripts = vector<CScript>());
    bool SpendAllZerocoins();

    /* POS functionality */

    bool ProcessStakingSettings(std::string &sError);

    bool InMempool(const uint256 &hash) const;

    CAmount GetStaked();
    size_t CountColdstakeOutputs();
    uint64_t GetStakeWeight() const;

    bool SetReserveBalance(CAmount nNewReserveBalance);
    void AvailableCoinsForStaking(std::vector<COutput> &vCoins, int64_t nTime, int nHeight) const;
    bool SelectCoinsForStaking(int64_t nTargetValue, int64_t nTime, int nHeight, std::set<std::pair<const CWalletTx*,unsigned int> >& setCoinsRet, int64_t& nValueRet) const;
    bool CreateCoinStake(unsigned int nBits, int64_t nTime, int nBlockHeight, int64_t nFees, CMutableTransaction &txNew, CKey &key, CBlockTemplate* pblocktemplate, int64_t nGhostFees, std::vector<unsigned char> &commitment, uint256 witnessroot);
    bool SignBlock(CBlockTemplate *pblocktemplate, int nHeight, int64_t nSearchTime);

    /* Return a script for a simple address type (normal/extended) */
    bool GetScriptForAddress(CScript &script, const CBitcoinAddress &addr, bool fUpdate = false, std::vector<uint8_t> *vData = NULL);

    boost::signals2::signal<void (CAmount nReservedBalance)> NotifyReservedBalanceChanged;

    int64_t nLastCoinStakeSearchTime = 0;
    int64_t nReserveBalance = 0;
    size_t nStakeThread = 9999999; // unset

    mutable int deepestTxnDepth = 0; // for stake mining

    mutable int m_greatest_txn_depth = 0; // depth of most deep txn
    //mutable int m_least_txn_depth = 0; // depth of least deep txn
    mutable bool m_have_spendable_balance_cached = false;
    mutable CAmount m_spendable_balance_cached = 0;

    enum eStakingState {
        NOT_STAKING = 0,
        IS_STAKING = 1,
        NOT_STAKING_BALANCE = -1,
        NOT_STAKING_DEPTH = -2,
        NOT_STAKING_LOCKED = -3,
        NOT_STAKING_LIMITED = -4,
        NOT_STAKING_DISABLED = -5,
        NOT_STAKING_NOT_UNLOCKED_FOR_STAKING_ONLY = -6,
    } nIsStaking = NOT_STAKING;


    // Staking Settings
    bool fStakingEnabled;
    CAmount nStakeCombineThreshold;
    CAmount nStakeSplitThreshold;
    CAmount nMinimumDelagatePercentage;
    std::vector<std::string> nDelegateRewardAddresses;
    bool nDelegateRewardToMe;
    size_t nMaxStakeCombine = 3;
    CAmount nWalletDonationPercent;
    std::string nWalletDonationAddress;
    int nStakeLimitHeight = 0; // for regtest, don't stake above nStakeLimitHeight

    bool fUnlockForStakingOnly = false; // Use coldstaking instead

    /**
     * Create a new transaction paying the recipients with a set of coins
     * selected by SelectCoins(); Also create the change output, when needed
     * @note passing nChangePosInOut as -1 will result in setting a random position
     */
    bool GetFeeForTransaction(const std::vector<CRecipient>& vecSend, CAmount& nFeeRet, int& nChangePosInOut,
                           std::string& strFailReason, const CCoinControl& coin_control, AvailableCoinsType nCoinType = ALL_COINS, bool fUseInstantSend = false);
    bool CreateTransaction(const std::vector<CRecipient>& vecSend, CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosInOut,
                           std::string& strFailReason, const CCoinControl& coin_control, bool sign = true, AvailableCoinsType nCoinType = ALL_COINS, bool fUseInstantSend = false);
    bool CommitTransaction(CWalletTx& wtxNew, CReserveKey& reservekey, CConnman* connman, CValidationState& state);

    bool CreateCollateralTransaction(CMutableTransaction& txCollateral, std::string& strReason);
    bool ConvertList(std::vector<CTxIn> vecTxIn, std::vector<CAmount>& vecAmounts);

    void ListAccountCreditDebit(const std::string& strAccount, std::list<CAccountingEntry>& entries);
    bool AddAccountingEntry(const CAccountingEntry&);
    bool AddAccountingEntry(const CAccountingEntry&, WalletBatch *batch);
    bool DummySignTx(CMutableTransaction &txNew, const std::set<CTxOut> &txouts, bool use_max_sig = false) const
    {
        std::vector<CTxOut> v_txouts(txouts.size());
        std::copy(txouts.begin(), txouts.end(), v_txouts.begin());
        return DummySignTx(txNew, v_txouts, use_max_sig);
    }
    bool DummySignTx(CMutableTransaction &txNew, const std::vector<CTxOut> &txouts, bool use_max_sig = false) const;
    bool DummySignInput(CTxIn &tx_in, const CTxOut &txout, bool use_max_sig = false) const;

    CFeeRate m_pay_tx_fee{DEFAULT_PAY_TX_FEE};
    unsigned int m_confirm_target{DEFAULT_TX_CONFIRM_TARGET};
    bool m_spend_zero_conf_change{DEFAULT_SPEND_ZEROCONF_CHANGE};
    bool m_signal_rbf{DEFAULT_WALLET_RBF};
    bool m_allow_fallback_fee{true}; //<! will be defined via chainparams
    CFeeRate m_min_fee{DEFAULT_TRANSACTION_MINFEE}; //!< Override with -mintxfee
    /**
     * If fee estimation does not have enough data to provide estimates, use this fee instead.
     * Has no effect if not using fee estimation
     * Override with -fallbackfee
     */
    CFeeRate m_fallback_fee{DEFAULT_FALLBACK_FEE};
    CFeeRate m_discard_rate{DEFAULT_DISCARD_FEE};
    OutputType m_default_address_type{DEFAULT_ADDRESS_TYPE};
    OutputType m_default_change_type{DEFAULT_CHANGE_TYPE};

    bool NewKeyPool();
    size_t KeypoolCountExternalKeys() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    bool TopUpKeyPool(unsigned int kpSize = 0);

    /**
     * Reserves a key from the keypool and sets nIndex to its index
     *
     * @param[out] nIndex the index of the key in keypool
     * @param[out] keypool the keypool the key was drawn from, which could be the
     *     the pre-split pool if present, or the internal or external pool
     * @param fRequestedInternal true if the caller would like the key drawn
     *     from the internal keypool, false if external is preferred
     *
     * @return true if succeeded, false if failed due to empty keypool
     * @throws std::runtime_error if keypool read failed, key was invalid,
     *     was not found in the wallet, or was misclassified in the internal
     *     or external keypool
     */
    bool ReserveKeyFromKeyPool(int64_t& nIndex, CKeyPool& keypool, bool fRequestedInternal);
    void KeepKey(int64_t nIndex);
    void ReturnKey(int64_t nIndex, bool fInternal, const CPubKey& pubkey);
    bool GetKeyFromPool(CPubKey &key, bool internal = false);
    isminetype HaveStealthAddress(const CStealthAddress &sxAddr) const;
    bool GetStealthAddressScanKey(CStealthAddress &sxAddr) const;

    bool ImportStealthAddress(const CStealthAddress &sxAddr, const CKey &skSpend);

    int64_t GetOldestKeyPoolTime();
    /**
     * Marks all keys in the keypool up to and including reserve_key as used.
     */
    void MarkReserveKeysAsUsed(int64_t keypool_id) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    const std::map<CKeyID, int64_t>& GetAllReserveKeys() const { return m_pool_key_to_index; }

    std::set<std::set<CTxDestination>> GetAddressGroupings() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);
    std::map<CTxDestination, CAmount> GetAddressBalances();

    std::set<CTxDestination> GetLabelAddresses(const std::string& label) const;
    void DeleteLabel(const std::string& label);

    isminetype IsMine(const CTxIn& txin) const;
    /**
     * Returns amount of debit if the input matches the
     * filter, otherwise returns 0
     */
    CAmount GetDebit(const CTxIn& txin, const isminefilter& filter) const;
    isminetype IsMine(const CTxOut& txout) const;
    CAmount GetCredit(const CTxOut& txout, const isminefilter& filter) const;
    bool IsChange(const CTxOut& txout) const;
    CAmount GetChange(const CTxOut& txout) const;
    bool IsMine(const CTransaction& tx) const;
    /** should probably be renamed to IsRelevantToMe */
    bool IsFromMe(const CTransaction& tx) const;
    CAmount GetDebit(const CTransaction& tx, const isminefilter& filter) const;
    /** Returns whether all of the inputs match the filter */
    bool IsAllFromMe(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetCredit(const CTransaction& tx, const isminefilter& filter) const;
    CAmount GetChange(const CTransaction& tx) const;
    void ChainStateFlushed(const CBlockLocator& loc) override;

    DBErrors LoadWallet(bool& fFirstRunRet);
    DBErrors ZapWalletTx(std::vector<CWalletTx>& vWtx);
    DBErrors ZapSelectTx(std::vector<uint256>& vHashIn, std::vector<uint256>& vHashOut) EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    bool SetAddressBook(const CTxDestination& address, const std::string& strName, const std::string& purpose, bool bech32 = false);
    bool DelAddressBook(const CTxDestination& address);

    const std::string& GetAccountName(const CScript& scriptPubKey) const;

    void GetScriptForMining(std::shared_ptr<CReserveScript> &script);

    unsigned int GetKeyPoolSize() EXCLUSIVE_LOCKS_REQUIRED(cs_wallet)
    {
        AssertLockHeld(cs_wallet); // set{Ex,In}ternalKeyPool
        return setInternalKeyPool.size() + setExternalKeyPool.size();
    }

    //! signify that a particular wallet feature is now used. this may change nWalletVersion and nWalletMaxVersion if those are lower
    void SetMinVersion(enum WalletFeature, WalletBatch* batch_in = nullptr, bool fExplicit = false);

    //! change which version we're allowed to upgrade to (note that this does not immediately imply upgrading to that format)
    bool SetMaxVersion(int nVersion);

    //! get the current wallet format (the oldest client version guaranteed to understand this wallet)
    int GetVersion() { LOCK(cs_wallet); return nWalletVersion; }

    //! Get wallet transactions that conflict with given transaction (spend same outputs)
    std::set<uint256> GetConflicts(const uint256& txid) const;

    //! Check if a given transaction has any of its outputs spent by another transaction in the wallet
    bool HasWalletSpend(const uint256& txid) const EXCLUSIVE_LOCKS_REQUIRED(cs_wallet);

    //! Flush wallet (bitdb flush)
    void Flush(bool shutdown=false);

    /** Wallet is about to be unloaded */
    boost::signals2::signal<void ()> NotifyUnload;

    /**
     * Address book entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const CTxDestination
            &address, const std::string &label, bool isMine,
            const std::string &purpose,
            ChangeType status)> NotifyAddressBookChanged;

    /**
     * Wallet transaction added, removed or updated.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const uint256 &hashTx,
            ChangeType status)> NotifyTransactionChanged;

    /**
     * Zerocoin entry changed.
     * @note called with lock cs_wallet held.
     */
    boost::signals2::signal<void (CWallet *wallet, const std::string &pubCoin, int denomination, const std::string &isUsed, ChangeType status)> NotifyZerocoinChanged;
    void NotifyGhostChanged(CWallet *wallet, const std::string &pubCoin, int denomination, const std::string &isUsed, ChangeType status);

    /** Show progress e.g. for rescan */
    boost::signals2::signal<void (const std::string &title, int nProgress)> ShowProgress;

    /** Watch-only address added */
    boost::signals2::signal<void (bool fHaveWatchOnly)> NotifyWatchonlyChanged;

    /** Inquire whether this wallet broadcasts transactions. */
    bool GetBroadcastTransactions() const { return fBroadcastTransactions; }
    /** Set whether this wallet broadcasts transactions. */
    void SetBroadcastTransactions(bool broadcast) { fBroadcastTransactions = broadcast; }

    /** Return whether transaction can be abandoned */
    bool TransactionCanBeAbandoned(const uint256& hashTx) const;

    /* Mark a transaction (and it in-wallet descendants) as abandoned so its inputs may be respent. */
    bool AbandonTransaction(const uint256& hashTx);

    /** Mark a transaction as replaced by another transaction (e.g., BIP 125). */
    bool MarkReplaced(const uint256& originalHash, const uint256& newHash);

    //! Verify wallet naming and perform salvage on the wallet if required
    static bool Verify(std::string wallet_file, bool salvage_wallet, std::string& error_string, std::string& warning_string);

    /* Initializes the wallet, returns a new CWallet instance or a null pointer in case of an error */
    static std::shared_ptr<CWallet> CreateWalletFromFile(const std::string& name, const fs::path& path, uint64_t wallet_creation_flags = 0);

    /**
     * Wallet post-init setup
     * Gives the wallet a chance to register repetitive tasks and complete post-init tasks
     */
    void postInitProcess();

    bool BackupWallet(const std::string& strDest);

    /* Set the HD chain model (chain child index counters) */
    void SetHDChain(const CHDChain& chain, bool memonly);
    const CHDChain& GetHDChain() const { return hdChain; }

    /* Returns true if HD is enabled */
    bool IsHDEnabled() const;

    /* Generates a new HD seed (will not be activated) */
    CPubKey GenerateNewSeed();

    /* Derives a new HD seed (will not be activated) */
    CPubKey DeriveNewSeed(const CKey& key);

    /* Set the current HD seed (will reset the chain child index counters)
       Sets the seed's version based on the current wallet version (so the
       caller must ensure the current wallet version is correct before calling
       this function). */
    void SetHDSeed(const CPubKey& key);

    /**
     * Blocks until the wallet state is up-to-date to /at least/ the current
     * chain at the time this function is entered
     * Obviously holding cs_main/cs_wallet when going into this call may cause
     * deadlock
     */
    void BlockUntilSyncedToCurrentChain() LOCKS_EXCLUDED(cs_wallet);

    /**
     * Explicitly make the wallet learn the related scripts for outputs to the
     * given key. This is purely to make the wallet file compatible with older
     * software, as CBasicKeyStore automatically does this implicitly for all
     * keys now.
     */
    void LearnRelatedScripts(const CPubKey& key, OutputType);

    /**
     * Same as LearnRelatedScripts, but when the OutputType is not known (and could
     * be anything).
     */
    void LearnAllRelatedScripts(const CPubKey& key);

    /** set a single wallet flag */
    void SetWalletFlag(uint64_t flags);

    /** check if a certain wallet flag is set */
    bool IsWalletFlagSet(uint64_t flag);

    /** overwrite all flags by the given uint64_t
       returns false if unknown, non-tolerable flags are present */
    bool SetWalletFlags(uint64_t overwriteFlags, bool memOnly);

    /** Returns a bracketed wallet name for displaying in logs, will return [default wallet] if the wallet has no name */
    const std::string GetDisplayName() const {
        std::string wallet_name = GetName().length() == 0 ? "default wallet" : GetName();
        return strprintf("[%s]", wallet_name);
    };

    /** Prepends the wallet name in logging output to ease debugging in multi-wallet use cases */
    template<typename... Params>
    void WalletLogPrintf(std::string fmt, Params... parameters) const {
        LogPrintf(("%s " + fmt).c_str(), GetDisplayName(), parameters...);
    };

};

/** A key allocated from the key pool. */
class CReserveKey final : public CReserveScript
{
protected:
    CWallet* pwallet;
    int64_t nIndex;
    CPubKey vchPubKey;
    bool fInternal;
public:
    explicit CReserveKey(CWallet* pwalletIn)
    {
        nIndex = -1;
        pwallet = pwalletIn;
        fInternal = false;
    }

    CReserveKey() = default;
    CReserveKey(const CReserveKey&) = delete;
    CReserveKey& operator=(const CReserveKey&) = delete;

    ~CReserveKey()
    {
        ReturnKey();
    }

    void ReturnKey();
    bool GetReservedKey(CPubKey &pubkey, bool internal = false);
    void KeepKey();
    void KeepScript() override { KeepKey(); }
};


/**
 * DEPRECATED Account information.
 * Stored in wallet with key "acc"+string account name.
 */
class CAccount
{
public:
    CPubKey vchPubKey;

    CAccount()
    {
        SetNull();
    }

    void SetNull()
    {
        vchPubKey = CPubKey();
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        int nVersion = s.GetVersion();
        if (!(s.GetType() & SER_GETHASH))
            READWRITE(nVersion);
        READWRITE(vchPubKey);
    }
};

// Helper for producing a bunch of max-sized low-S signatures (eg 72 bytes)
// ContainerType is meant to hold pair<CWalletTx *, int>, and be iterable
// so that each entry corresponds to each vIn, in order.
template <typename ContainerType>
bool CWallet::DummySignTx(CMutableTransaction &txNew, const ContainerType &coins) const
{
    // Fill in dummy signatures for fee calculation.
    int nIn = 0;
    for (const auto& coin : coins)
    {
        CScript scriptPubKeyOut = coin.txout.scriptPubKey;

        //check if this is a coldstake
        if ((HasIsCoinstakeOp(scriptPubKeyOut)))
        {
            CScript nonCoinstakePath;
            if (!GetNonCoinstakeScriptPath(scriptPubKeyOut, nonCoinstakePath))
                return error("%s: Cannot retrieve non-coinstake script.", __func__);;
            scriptPubKeyOut = nonCoinstakePath;
        }

        const CScript& scriptPubKey = scriptPubKeyOut;
        SignatureData sigdata;

        if (!ProduceSignature(DummySignatureCreator(this), scriptPubKey, sigdata))
        {
            return false;
        } else {
            UpdateTransaction(txNew, nIn, sigdata);
        }

        nIn++;
    }
    return true;
}

OutputType ParseOutputType(const std::string& str, OutputType default_type = OUTPUT_TYPE_DEFAULT);
const std::string& FormatOutputType(OutputType type);

/**
 * Get a destination of the requested type (if possible) to the specified key.
 * The caller must make sure LearnRelatedScripts has been called beforehand.
 */
CTxDestination GetDestinationForKey(const CPubKey& key, OutputType);

/** Get all destinations (potentially) supported by the wallet for the given key. */
std::vector<CTxDestination> GetAllDestinationsForKey(const CPubKey& key);

/** RAII object to check and reserve a wallet rescan */
class WalletRescanReserver
{
private:
    CWallet* m_wallet;
    bool m_could_reserve;
public:
    explicit WalletRescanReserver(CWallet* w) : m_wallet(w), m_could_reserve(false) {}

    bool reserve()
    {
        assert(!m_could_reserve);
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_wallet->fScanningWallet) {
            return false;
        }
        m_wallet->fScanningWallet = true;
        m_could_reserve = true;
        return true;
    }

    bool isReserved() const
    {
        return (m_could_reserve && m_wallet->fScanningWallet);
    }

    ~WalletRescanReserver()
    {
        std::lock_guard<std::mutex> lock(m_wallet->mutexScanning);
        if (m_could_reserve) {
            m_wallet->fScanningWallet = false;
        }
    }
};

// Calculate the size of the transaction assuming all signatures are max size
// Use DummySignatureCreator, which inserts 71 byte signatures everywhere.
// NOTE: this requires that all inputs must be in mapWallet (eg the tx should
// be IsAllFromMe).
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, bool use_max_sig = false);
int64_t CalculateMaximumSignedTxSize(const CTransaction &tx, const CWallet *wallet, const std::vector<CTxOut>& txouts, bool use_max_sig = false);
#endif // BITCOIN_WALLET_WALLET_H
