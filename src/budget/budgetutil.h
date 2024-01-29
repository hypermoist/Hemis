// Copyright (c) 2021 The Hemis Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://www.opensource.org/licenses/mit-license.php.

#ifndef Hemis_BUDGETUTIL_H
#define Hemis_BUDGETUTIL_H

#include "uint256.h"
#include "budget/budgetvote.h"

#include <string>
#include <list>

// Future: Decouple UniValue usage. Should be used only in the RPC server files for the inputs/outputs values.

class CWallet;

// vote on proposal (finalized budget, if fFinal=true) with the active local gamemaster
// Note: for DGMs only finalized budget voting is allowed with the operator key
// (proposal voting requires the voting key)
UniValue gmLocalBudgetVoteInner(bool fLegacyGM, const uint256& budgetHash, bool fFinal,
                                       const CBudgetVote::VoteDirection& nVote);

UniValue gmBudgetVoteInner(CWallet* const pwallet, bool fLegacyGM, const uint256& budgetHash, bool fFinal,
                                  const CBudgetVote::VoteDirection& nVote, const Optional<std::string>& mnAliasFilter);

#endif //Hemis_BUDGETUTIL_H
