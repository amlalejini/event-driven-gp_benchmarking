#ifndef SGP_ADVENTURE_TOOLBELT_H
#define SGP_ADVENTURE_TOOLBELT_H

#include <iostream>
#include <string>
#include <utility>
#include <fstream>
#include <algorithm>
#include <functional>
#include <unordered_set>
#include <unordered_map>

#include "base/Ptr.h"
#include "base/vector.h"
#include "hardware/EventDrivenGP.h"
#include "tools/BitVector.h"
#include "tools/Random.h"
#include "tools/random_utils.h"
#include "tools/math.h"
#include "tools/string_utils.h"

namespace toolbelt {

  /// Generate random tags. Can guarantee uniqueness. 
  template<size_t TAG_WIDTH>
  emp::vector<typename emp::EventDrivenGP_AW<TAG_WIDTH>::affinity_t> GenerateRandomTags(emp::Random & rnd, size_t tag_cnt, bool guarantee_unique=true) {
    using tag_t = typename emp::EventDrivenGP_AW<TAG_WIDTH>::affinity_t;
    if (guarantee_unique) {
      if (tag_cnt > emp::Pow2(TAG_WIDTH)) {
        std::cout << "Number of unique tags (" << tag_cnt << ") exceeds limit! Exiting..." << std::endl;
        exit(-1);
      }
    }
    std::unordered_set<int> uset; // We'll use this to ensure all maze tags are unique.
    emp::vector<tag_t> tags;
    for (size_t i = 0; i < tag_cnt; ++i) {
      tags.emplace_back(tag_t());
      tags[i].Randomize(rnd);
      if (guarantee_unique) {
        // If we guarantee uniqueness, keep regenerating tags until we haven't seen one before. 
        int tag_int = tags[i].GetUInt(0);
        while (true) {
          if (!emp::Has(uset, tag_int)) {
            uset.emplace(tag_int);
            break;
          } else {
            tags[i].Randomize(rnd);
            tag_int = tags[i].GetUInt(0);
          }
        }
      }
    }
    return tags;
  }

  /// SignalGPMutator implements the standard mutation function that I use for 
  /// most SignalGP experiments.
  // TODO: 
  //  [ ] Allow for reconfigurable mutation function
  //  [ ] Add various signals to ApplyMutation function
  //  [ ] track how many of each type of mutation occurs
  template <typename HARDWARE>
  class SignalGPMutator {
    using hardware_t = HARDWARE;
    using program_t = typename hardware_t::program_t;
    using tag_t = typename hardware_t::affinity_t;
    using inst_t = typename hardware_t::inst_t;
    using function_t = typename hardware_t::Function;

    protected:
      size_t PROG_MIN_FUNC_CNT;
      size_t PROG_MAX_FUNC_CNT;
      size_t PROG_MIN_FUNC_LEN;
      size_t PROG_MAX_FUNC_LEN;
      size_t PROG_MAX_TOTAL_LEN;

      int PROG_MAX_ARG_VAL;

      double PER_BIT__TAG_BFLIP_RATE;
      double PER_INST__SUB_RATE;
      double PER_INST__INS_RATE;
      double PER_INST__DEL_RATE;
      double PER_FUNC__SLIP_RATE;
      double PER_FUNC__FUNC_DUP_RATE;
      double PER_FUNC__FUNC_DEL_RATE;

    public:
      SignalGPMutator(size_t _PROG_MIN_FUNC_CNT=1,
                      size_t _PROG_MAX_FUNC_CNT=8,
                      size_t _PROG_MIN_FUNC_LEN=1,
                      size_t _PROG_MAX_FUNC_LEN=8,
                      size_t _PROG_MAX_TOTAL_LEN=64,
                      int _PROG_MAX_ARG_VAL=16,
                      double _PER_BIT__TAG_BFLIP_RATE=0.005,
                      double _PER_INST__SUB_RATE=0.005,
                      double _PER_INST__INS_RATE=0.005,
                      double _PER_INST__DEL_RATE=0.005,
                      double _PER_FUNC__SLIP_RATE=0.05,
                      double _PER_FUNC__FUNC_DUP_RATE=0.05,
                      double _PER_FUNC__FUNC_DEL_RATE=0.05)
        : PROG_MIN_FUNC_CNT(_PROG_MIN_FUNC_CNT),
          PROG_MAX_FUNC_CNT(_PROG_MAX_FUNC_CNT),
          PROG_MIN_FUNC_LEN(_PROG_MIN_FUNC_LEN),
          PROG_MAX_FUNC_LEN(_PROG_MAX_FUNC_LEN),
          PROG_MAX_TOTAL_LEN(_PROG_MAX_TOTAL_LEN),
          PROG_MAX_ARG_VAL(_PROG_MAX_ARG_VAL),
          PER_BIT__TAG_BFLIP_RATE(_PER_BIT__TAG_BFLIP_RATE),
          PER_INST__SUB_RATE(_PER_INST__SUB_RATE),
          PER_INST__INS_RATE(_PER_INST__INS_RATE),
          PER_INST__DEL_RATE(_PER_INST__DEL_RATE),
          PER_FUNC__SLIP_RATE(_PER_FUNC__SLIP_RATE),
          PER_FUNC__FUNC_DUP_RATE(_PER_FUNC__FUNC_DUP_RATE),
          PER_FUNC__FUNC_DEL_RATE(_PER_FUNC__FUNC_DEL_RATE) 
      { ; }

      ~SignalGPMutator() { ; }

      size_t GetProgMinFuncCnt() const { return PROG_MIN_FUNC_CNT; }
      size_t GetProgMaxFuncCnt() const { return PROG_MAX_FUNC_CNT; }
      size_t GetProgMinFuncLen() const { return PROG_MIN_FUNC_LEN; }
      size_t GetProgMaxFuncLen() const { return PROG_MAX_FUNC_LEN; }
      size_t GetProgMaxTotalLen() const { return PROG_MAX_TOTAL_LEN; }
      int GetProgMaxArgVal() const { return PROG_MAX_ARG_VAL; }
      double GetPerBitTagBitFlipRate()  const { return PER_BIT__TAG_BFLIP_RATE; }
      double GetPerInstSubRate() const { return PER_INST__SUB_RATE; }
      double GetPerInstInsRate() const { return PER_INST__INS_RATE; }
      double GetPerInstDelRate() const { return PER_INST__DEL_RATE; }
      double GetPerFuncSlipRate() const { return PER_FUNC__SLIP_RATE; }
      double GetPerFuncDupRate() const { return PER_FUNC__FUNC_DUP_RATE; }
      double GetPerFuncDelRate() const { return PER_FUNC__FUNC_DEL_RATE; }

      // TODO: add value guards/emp_asserts!
      void SetProgMinFuncCnt(size_t val) { PROG_MIN_FUNC_CNT = val; }
      void SetProgMaxFuncCnt(size_t val) { PROG_MAX_FUNC_CNT = val; }
      void SetProgMinFuncLen(size_t val) { PROG_MIN_FUNC_LEN = val; }
      void SetProgMaxFuncLen(size_t val) { PROG_MAX_FUNC_LEN = val; }
      void SetProgMaxTotalLen(size_t val) { PROG_MAX_TOTAL_LEN = val; }
      void SetProgMaxArgVal(int val) { PROG_MAX_ARG_VAL = val; }
      void SetPerBitTagBitFlipRate(double val) { PER_BIT__TAG_BFLIP_RATE = val; }
      void SetPerInstSubRate(double val) { PER_INST__SUB_RATE = val; }
      void SetPerInstInsRate(double val) { PER_INST__INS_RATE = val; }
      void SetPerInstDelRate(double val) { PER_INST__DEL_RATE = val; }
      void SetPerFuncSlipRate(double val) { PER_FUNC__SLIP_RATE = val; }
      void SetPerFuncDupRate(double val) { PER_FUNC__FUNC_DUP_RATE = val; }
      void SetPerFuncDelRate(double val) { PER_FUNC__FUNC_DEL_RATE = val; }

      size_t ApplyMutations(program_t & program, emp::Random & rnd) {
        size_t mut_cnt = 0;
        size_t expected_prog_len = program.GetInstCnt();

        // Duplicate a (single) function?
        if (rnd.P(PER_FUNC__FUNC_DUP_RATE) && program.GetSize() < PROG_MAX_FUNC_CNT)
        {
          const uint32_t fID = rnd.GetUInt(program.GetSize());
          // Would function duplication make expected program length exceed max?
          if (expected_prog_len + program[fID].GetSize() <= PROG_MAX_TOTAL_LEN)
          {
            program.PushFunction(program[fID]);
            expected_prog_len += program[fID].GetSize();
            ++mut_cnt;
          }
        }

        // Delete a (single) function?
        if (rnd.P(PER_FUNC__FUNC_DEL_RATE) && program.GetSize() > PROG_MIN_FUNC_CNT)
        {
          const uint32_t fID = rnd.GetUInt(program.GetSize());
          expected_prog_len -= program[fID].GetSize();
          program[fID] = program[program.GetSize() - 1];
          program.program.resize(program.GetSize() - 1);
          ++mut_cnt;
        }

        // For each function...
        for (size_t fID = 0; fID < program.GetSize(); ++fID)
        {

          // Mutate affinity
          for (size_t i = 0; i < program[fID].GetAffinity().GetSize(); ++i)
          {
            tag_t &aff = program[fID].GetAffinity();
            if (rnd.P(PER_BIT__TAG_BFLIP_RATE))
            {
              ++mut_cnt;
              aff.Set(i, !aff.Get(i));
            }
          }

          // Slip-mutation?
          if (rnd.P(PER_FUNC__SLIP_RATE))
          {
            uint32_t begin = rnd.GetUInt(program[fID].GetSize());
            uint32_t end = rnd.GetUInt(program[fID].GetSize());
            const bool dup = begin < end;
            const bool del = begin > end;
            const int dup_size = end - begin;
            const int del_size = begin - end;
            // If we would be duplicating and the result will not exceed maximum program length, duplicate!
            if (dup && (expected_prog_len + dup_size <= PROG_MAX_TOTAL_LEN) && (program[fID].GetSize() + dup_size <= PROG_MAX_FUNC_LEN))
            {
              // duplicate begin:end
              const size_t new_size = program[fID].GetSize() + (size_t)dup_size;
              function_t new_fun(program[fID].GetAffinity());
              for (size_t i = 0; i < new_size; ++i)
              {
                if (i < end)
                  new_fun.PushInst(program[fID][i]);
                else
                  new_fun.PushInst(program[fID][i - dup_size]);
              }
              program[fID] = new_fun;
              ++mut_cnt;
              expected_prog_len += dup_size;
            }
            else if (del && ((program[fID].GetSize() - del_size) >= PROG_MIN_FUNC_LEN))
            {
              // delete end:begin
              function_t new_fun(program[fID].GetAffinity());
              for (size_t i = 0; i < end; ++i)
                new_fun.PushInst(program[fID][i]);
              for (size_t i = begin; i < program[fID].GetSize(); ++i)
                new_fun.PushInst(program[fID][i]);
              program[fID] = new_fun;
              ++mut_cnt;
              expected_prog_len -= del_size;
            }
          }

          // Substitution mutations? (pretty much completely safe)
          for (size_t i = 0; i < program[fID].GetSize(); ++i)
          {
            inst_t &inst = program[fID][i];
            // Mutate affinity (even when it doesn't use it).
            for (size_t k = 0; k < inst.affinity.GetSize(); ++k)
            {
              if (rnd.P(PER_BIT__TAG_BFLIP_RATE))
              {
                ++mut_cnt;
                inst.affinity.Set(k, !inst.affinity.Get(k));
              }
            }

            // Mutate instruction.
            if (rnd.P(PER_INST__SUB_RATE))
            {
              ++mut_cnt;
              inst.id = rnd.GetUInt(program.GetInstLib()->GetSize());
            }

            // Mutate arguments (even if they aren't relevent to instruction).
            for (size_t k = 0; k < hardware_t::MAX_INST_ARGS; ++k)
            {
              if (rnd.P(PER_INST__SUB_RATE))
              {
                ++mut_cnt;
                inst.args[k] = rnd.GetInt(PROG_MAX_ARG_VAL);
              }
            }
          }

          // Insertion/deletion mutations?
          // - Compute number of insertions.
          int num_ins = rnd.GetRandBinomial(program[fID].GetSize(), PER_INST__INS_RATE);
          // Ensure that insertions don't exceed maximum program length.
          if ((num_ins + program[fID].GetSize()) > PROG_MAX_FUNC_LEN)
          {
            num_ins = PROG_MAX_FUNC_LEN - program[fID].GetSize();
          }
          if ((num_ins + expected_prog_len) > PROG_MAX_TOTAL_LEN)
          {
            num_ins = PROG_MAX_TOTAL_LEN - expected_prog_len;
          }
          expected_prog_len += num_ins;

          // Do we need to do any insertions or deletions?
          if (num_ins > 0 || PER_INST__DEL_RATE > 0.0)
          {
            size_t expected_func_len = num_ins + program[fID].GetSize();
            // Compute insertion locations and sort them.
            emp::vector<size_t> ins_locs = emp::RandomUIntVector(rnd, num_ins, 0, program[fID].GetSize());
            if (ins_locs.size())
              std::sort(ins_locs.begin(), ins_locs.end(), std::greater<size_t>());
            function_t new_fun(program[fID].GetAffinity());
            size_t rhead = 0;
            while (rhead < program[fID].GetSize())
            {
              if (ins_locs.size())
              {
                if (rhead >= ins_locs.back())
                {
                  // Insert a random instruction.
                  new_fun.PushInst(rnd.GetUInt(program.GetInstLib()->GetSize()),
                                  rnd.GetInt(PROG_MAX_ARG_VAL),
                                  rnd.GetInt(PROG_MAX_ARG_VAL),
                                  rnd.GetInt(PROG_MAX_ARG_VAL),
                                  tag_t());
                  new_fun.inst_seq.back().affinity.Randomize(rnd);
                  ++mut_cnt;
                  ins_locs.pop_back();
                  continue;
                }
              }
              // Do we delete this instruction?
              if (rnd.P(PER_INST__DEL_RATE) && (expected_func_len > PROG_MIN_FUNC_LEN))
              {
                ++mut_cnt;
                --expected_prog_len;
                --expected_func_len;
              }
              else
              {
                new_fun.PushInst(program[fID][rhead]);
              }
              ++rhead;
            }
            program[fID] = new_fun;
          }
        }
        return mut_cnt;
      }

  };

}

#endif