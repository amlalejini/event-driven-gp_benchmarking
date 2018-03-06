/// Return number of mutation *events* that occur (e.g. function duplication, slip mutation are single events).
size_t Experiment::Mutate(Agent & agent, emp::Random & rnd) {
  program_t & program = agent.GetGenome();
  size_t mut_cnt = 0;
  // Duplicate a function?
  size_t expected_prog_len = program.GetInstCnt();
  size_t old_content_wall = program.GetSize(); ///< First position (or invalid position) after old content.
  size_t fID = 0;
  while (fID < old_content_wall) {
    bool dup = rnd.P(SGP__PER_FUNC__FUNC_DUP_RATE);
    bool del = rnd.P(SGP__PER_FUNC__FUNC_DEL_RATE);
    if (dup && del) { dup = false; del = false; } // If we're about to dup and del, don't do anything.
    // Do we duplicate?
    if (dup && ((expected_prog_len + program[fID].GetSize()) <= SGP_PROG_MAX_LEN) && program.GetSize() < SGP_FUNCTION_CNT) {
      // Adjust expected program length (total instructions).
      expected_prog_len += program[fID].GetSize();
      // Duplication.
      program.PushFunction(program[fID]);
      ++mut_cnt;
    // Do we delete?
    } else if (del && program.GetSize() > 1) {
      // Adjust expected program length (total instructions).
      expected_prog_len -= program[fID].GetSize();
      const size_t mfID = program.GetSize()-1;
      // Deletion.
      program[fID] = program[mfID];
      program.program.resize(mfID);
      // Should we adjust the wall?
      if (mfID < old_content_wall) {
        // We're moving from within the wall, adjust wall.
        --old_content_wall;
        --fID;
      }
      ++mut_cnt;
    }
    ++fID;
  }
  // For each function...
  for (size_t fID = 0; fID < program.GetSize(); ++fID) {
    // Mutate affinity
    for (size_t i = 0; i < program[fID].GetAffinity().GetSize(); ++i) {
      tag_t & aff = program[fID].GetAffinity();
      if (rnd.P(SGP__PER_BIT__TAG_BFLIP_RATE)) {
        ++mut_cnt;
        aff.Set(i, !aff.Get(i));
      }
    }
    // Slip-mutation?
    if (rnd.P(SGP__PER_FUNC__SLIP_RATE)) {
      uint32_t begin = rnd.GetUInt(program[fID].GetSize());
      uint32_t end = rnd.GetUInt(program[fID].GetSize());
      const bool dup = begin < end;
      const bool del = begin > end;
      // If we would be duplicating and the result will not exceed maximum program length, duplicate!
      if (dup && (expected_prog_len+(end-begin) <= SGP_PROG_MAX_LEN)) {
        // duplicate begin:end
        ++mut_cnt;
        const size_t dup_size = end - begin;
        const size_t new_size = program[fID].GetSize() + dup_size;
        expected_prog_len += dup_size;
        hardware_t::Function new_fun(program[fID].GetAffinity());
        for (size_t i = 0; i < new_size; ++i) {
          if (i < end) new_fun.PushInst(program[fID][i]);
          else new_fun.PushInst(program[fID][i - dup_size]);
        }
        program[fID] = new_fun;
      } else if (del && ((program[fID].GetSize() - (begin - end)) >= 1)) {
        // delete end:begin
        ++mut_cnt;
        expected_prog_len -= (begin - end);
        hardware_t::Function new_fun(program[fID].GetAffinity());
        for (size_t i = 0; i < end; ++i)
          new_fun.PushInst(program[fID][i]);
        for (size_t i = begin; i < program[fID].GetSize(); ++i)
          new_fun.PushInst(program[fID][i]);
        program[fID] = new_fun;
      }
    }
    // Substitution mutations?
    for (size_t i = 0; i < program[fID].GetSize(); ++i) {
      inst_t & inst = program[fID][i];
      // Mutate affinity (even if it doesn't have one).
      for (size_t k = 0; k < inst.affinity.GetSize(); ++k) {
        if (rnd.P(SGP__PER_BIT__TAG_BFLIP_RATE)) {
          ++mut_cnt;
          inst.affinity.Set(k, !inst.affinity.Get(k));
        }
      }
      // Mutate instruction.
      if (rnd.P(SGP__PER_INST__SUB_RATE)) {
        ++mut_cnt;
        inst.id = rnd.GetUInt(program.GetInstLib()->GetSize());
      }
      // Mutate arguments (even if they aren't relevent to instruction).
      for (size_t k = 0; k < hardware_t::MAX_INST_ARGS; ++k) {
        if (rnd.P(SGP__PER_INST__SUB_RATE)) {
          ++mut_cnt;
          inst.args[k] = rnd.GetInt(SGP__PROG_MAX_ARG_VAL);
        }
      }
    }
    // Insertion/deletion mutations?
    // - Compute insertions.
    int num_ins = rnd.GetRandBinomial(program[fID].GetSize(), SGP__PER_INST__INS_RATE);
    // Ensure that insertions don't exceed maximum program length.
    if ((num_ins + expected_prog_len) > SGP_PROG_MAX_LEN) {
      num_ins = SGP_PROG_MAX_LEN - expected_prog_len;
    }
    expected_prog_len += num_ins;

    // Do we need to do any insertions or deletions?
    if (num_ins > 0 || SGP__PER_INST__DEL_RATE > 0.0) {
      emp::vector<size_t> ins_locs = emp::RandomUIntVector(rnd, num_ins, 0, program[fID].GetSize());
      if (ins_locs.size()) std::sort(ins_locs.begin(), ins_locs.end(), std::greater<size_t>());
      hardware_t::Function new_fun(program[fID].GetAffinity());
      size_t rhead = 0;
      size_t num_dels = 0;
      while (rhead < program[fID].GetSize()) {
        if (ins_locs.size()) {
          if (rhead >= ins_locs.back()) {
            // Insert a random instruction.
            new_fun.PushInst(rnd.GetUInt(program.GetInstLib()->GetSize()),
                             rnd.GetInt(SGP__PROG_MAX_ARG_VAL),
                             rnd.GetInt(SGP__PROG_MAX_ARG_VAL),
                             rnd.GetInt(SGP__PROG_MAX_ARG_VAL),
                             tag_t());
            new_fun.inst_seq.back().affinity.Randomize(rnd);
            ++mut_cnt;
            ins_locs.pop_back();
            continue;
          }
        }
        // Do we delete this instruction?
        if (rnd.P(SGP__PER_INST__DEL_RATE) && num_dels < (program[fID].GetSize() - 1)) {
          ++mut_cnt;
          ++num_dels;
          --expected_prog_len;
        } else {
          new_fun.PushInst(program[fID][rhead]);
        }
        ++rhead;
      }
      program[fID] = new_fun;
    }
  }
  return mut_cnt;
}
