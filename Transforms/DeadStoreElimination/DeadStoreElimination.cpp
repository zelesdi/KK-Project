#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"

#include <utility>
#include <vector>

using namespace llvm;
using namespace std;

namespace{

bool UnknownMemory(Instruction &I){
    if (isa<FenceInst>(I))
        return true;
    
    if (auto *CB = dyn_cast<CallBase>(&I)){
        if (auto *II = dyn_cast<IntrinsicInst>(CB)){
            if (II->doesNotAccessMemory())
                return false;
        }
        return true;
    }

    return false;
}

class SimpleDSE: public FunctionPass{
public:
    static char ID;

    SimpleDSE() : FunctionPass(ID) {}

    bool runOnFunction(Function &F) override{
        if(skipFunction(F))
            return false;
        
        AliasAnalysis &AA = getAnalysis<AAResultsWrapperPass>().getAAResults();
        
        LiveIn.clear();
        AllLiveIn.clear();
        LiveOut.clear();
        AllLiveOut.clear();

        computeCrossBlockLiveness(F, AA);

        return eliminateDeadStores(F, AA);
    }

private:

    DenseMap<BasicBlock *, vector<MemoryLocation>> LiveIn;
    DenseMap<BasicBlock *, bool> AllLiveIn;
    DenseMap<BasicBlock *, vector<MemoryLocation>> LiveOut;
    DenseMap<BasicBlock *, bool> AllLiveOut;
    

    static void killMustAliased(vector<MemoryLocation> &Locs, const MemoryLocation &Killer, AliasAnalysis &AA){

        vector<MemoryLocation> Kept;
        Kept.reserve(Locs.size());
        for (const MemoryLocation &L : Locs){
            if (AA.alias(L, Killer) != AliasResult::MustAlias)
                Kept.push_back(L);
        }
        Locs = std::move(Kept);
    }

    static bool mayAliasAny(const MemoryLocation &Loc, const vector<MemoryLocation> &Locs, AliasAnalysis &AA){
        for (const MemoryLocation &L : Locs){
            if (AA.alias(Loc, L) != AliasResult::NoAlias)
                return true;
        }
        return false;
    }

    void computeCrossBlockLiveness(Function &F, AliasAnalysis &AA){
        vector<BasicBlock *> WorkList;
        DenseSet<BasicBlock *> InWorkList;

        for (BasicBlock &BB : F){
            LiveIn[&BB] = {};
            AllLiveIn[&BB] = false;
            WorkList.push_back(&BB);
            InWorkList.insert(&BB);
        }

        while (!WorkList.empty()){
            BasicBlock *BB = WorkList.back();
            WorkList.pop_back();
            InWorkList.erase(BB);

            vector<MemoryLocation> NewLiveOut;
            bool NewAllLiveOut = false;

            for(BasicBlock *Succ : successors(BB)){
                if (AllLiveIn[Succ])
                    NewAllLiveOut = true;
                for (const MemoryLocation &Loc : LiveIn[Succ])
                    NewLiveOut.push_back(Loc);
            }

            LiveOut[BB] = NewLiveOut;
            AllLiveOut[BB] = NewAllLiveOut;

            vector<MemoryLocation> CurLive = NewLiveOut;
            bool CurAllLive = NewAllLiveOut;

            for (auto it = BB->rbegin(); it != BB->rend(); ++it){
                Instruction &I = *it;

                if(auto *LI = dyn_cast<LoadInst>(&I)){
                    if (LI->isVolatile()){
                        CurAllLive = true;
                        continue;
                    }
                
                    if (!CurAllLive) {
                        MemoryLocation Loc = MemoryLocation::get(LI);
                        if (!mayAliasAny(Loc, CurLive, AA))
                        CurLive.push_back(Loc);
                    }
                    continue;
                }

                if (auto *SI = dyn_cast<StoreInst>(&I)) {
                    if (SI->isVolatile()) {
                        CurAllLive = true;
                        continue;
                }
                if (!CurAllLive) {
                    MemoryLocation Loc = MemoryLocation::get(SI);
                    killMustAliased(CurLive, Loc, AA);
                    }
                    continue;
                }

                if (UnknownMemory(I)) {
                    CurAllLive = true;
                    continue;
                }
           }

           bool Changed = false;

           if (CurAllLive && !AllLiveIn[BB]){
                AllLiveIn[BB] = true;
                Changed = true;
           }

           if (CurLive.size() > LiveIn[BB].size()){
                LiveIn[BB] = CurLive;
                Changed = true;
           }

           if (Changed){
            for (BasicBlock *Pred : predecessors(BB)){
                if (InWorkList.insert(Pred).second)
                    WorkList.push_back(Pred);
            }
           }
        }
    }

bool eliminateDeadStores(Function &F, AliasAnalysis &AA){

        bool Changed = false;

        for(BasicBlock &BB : F){
            vector<pair<MemoryLocation, StoreInst *>> LastStores;
            vector<StoreInst *> ToRemove;

            for(Instruction &I : BB){
                if (auto *SI = dyn_cast<StoreInst>(&I)){
                    if (SI->isVolatile()){
                        LastStores.clear();
                        continue;
                    }
                
                    MemoryLocation Loc = MemoryLocation::get(SI);

                    for (auto It = LastStores.begin(); It != LastStores.end();){
                        AliasResult AR = AA.alias(It->first, Loc);
                        if(AR == AliasResult::MustAlias){
                            ToRemove.push_back(It->second);
                            It = LastStores.erase(It);
                        }else if (AR == AliasResult::NoAlias){
                            ++It;
                        }else{
                            It = LastStores.erase(It);
                        }

                    }

                    LastStores.emplace_back(Loc, SI);
                    continue;
                
                }
                if (auto *LI = dyn_cast<LoadInst>(&I)){
                    if (LI->isVolatile()){
                        LastStores.clear();
                        continue;
                    }

                    MemoryLocation Loc = MemoryLocation::get(LI);

                    for (auto It = LastStores.begin(); It != LastStores.end();){
                        if(AA.alias(It->first, Loc) != AliasResult::NoAlias)
                            It = LastStores.erase(It);
                        else
                            ++It;
                    }
                    continue;
                }
                
                if (UnknownMemory(I)){
                    LastStores.clear();
                    continue;
                }

            }

            bool BlockAllLive = AllLiveOut[&BB];
            const vector<MemoryLocation> &BlockLiveOut = LiveOut[&BB];

            if (!BlockAllLive){
                for (auto &Entry : LastStores){
                    if (!mayAliasAny(Entry.first, BlockLiveOut, AA)){
                        ToRemove.push_back(Entry.second);
                    }
                }
            }
            
            for (StoreInst *SI : ToRemove){
                SI->eraseFromParent();
                Changed = true;
            }

        }


        return Changed;

    }
    
    void getAnalysisUsage(AnalysisUsage &AU) const override{
        AU.addRequired<AAResultsWrapperPass>();
        AU.setPreservesCFG();
    }

};
}

char SimpleDSE::ID = 0;

static RegisterPass<SimpleDSE>X("simple-dse", "Simple Dead Store Elimination", false, false);
