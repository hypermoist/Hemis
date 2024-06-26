// Copyright (c) 2018-2019 The Dash Core developers
// Copyright (c) 2022 The Hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef Hemis_QUORUMS_CONNECTIONS_H
#define Hemis_QUORUMS_CONNECTIONS_H

#include "consensus/params.h"
#include "quorums.h"

class CBlockIndex;
class CDeterministicGM;
typedef std::shared_ptr<const CDeterministicGM> CDeterministicGMCPtr;

namespace llmq {

// Deterministically selects which node should initiate the gmauth process
uint256 DeterministicOutboundConnection(const uint256& proTxHash1, const uint256& proTxHash2);

// Return the outbound quorum relay members for 'forMember' (proTxHash)
std::set<uint256> GetQuorumRelayMembers(const std::vector<CDeterministicGMCPtr>& gmList, unsigned int forMemberIndex);
std::set<size_t> CalcDeterministicWatchConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, size_t memberCount, size_t connectionCount);

void EnsureQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& myProTxHash);
void EnsureLatestQuorumConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexNew, const uint256& myProTxHash, std::vector<CQuorumCPtr>& lastQuorums);

void AddQuorumProbeConnections(Consensus::LLMQType llmqType, const CBlockIndex* pindexQuorum, const uint256& myProTxHash);

} // namespace llmq

#endif // Hemis_QUORUMS_CONNECTIONS_H
