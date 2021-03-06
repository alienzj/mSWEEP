#include "read_bitfield.hpp"

#include <sstream>
#include <unordered_map>
#include <exception>

#include "bxzstr.hpp"
#include "file.hpp"

#include "tools/matchfasta.hpp"

void VerifyGrouping(const unsigned n_refs, std::istream &run_info) {
  // Get the number of reference sequences in the pseudoalignment
  // contained in the 'n_targets' variable in run_info.json file.
  short unsigned line_nr = 0; // number of reference seqs is on line 2 (kallisto v0.43)
  if (run_info.good()) {
    std::string line;
    while (getline(run_info, line)) {
      if (line_nr == 0) {
	++line_nr;
      } else {
	std::string part;
	std::stringstream partition(line);
	unsigned n_targets = 0;
	while (getline(partition, part, ':')) {
	  if (n_targets == 0) {
	    ++n_targets;
	  } else {
	    part.pop_back(); // the number ends in a ','; get rid of it.
	    unsigned n_targets = std::stoi(part);
	    if (n_targets > n_refs) {
	      throw std::runtime_error("pseudoalignment has more reference sequences than the grouping.");
	    } else if (n_targets < n_refs) {
	      throw std::runtime_error("grouping has more reference sequences than the pseudoalignment.");
	    }
	    return;
	  }
	}
      }
    }
  } else {
    throw std::runtime_error("Could not read run_info.json found.");
  }
}

uint32_t CountLines (std::istream &stream) {
  uint32_t n_lines = 0;
  std::string line;
  while (std::getline(stream, line)) {
    n_lines += 1;
  }
  return n_lines;
}

void VerifyThemistoGrouping(const unsigned n_refs, std::istream &themisto_index) {
  uint32_t lines_in_grouping = CountLines(themisto_index);
  if (lines_in_grouping > n_refs) {
    throw std::runtime_error("pseudoalignment has more reference sequences than the grouping.");
  } else if (lines_in_grouping < n_refs) {
    throw std::runtime_error("grouping has more reference sequences than the pseudoalignment.");
  }
}

void ReadClusterIndicators(std::istream &indicator_file, Reference &reference) {
  std::unordered_map<std::string, unsigned> str_to_int;

  if (indicator_file.good()) {
    std::string indicator_s;
    unsigned indicator_i = 0;
    while (getline(indicator_file, indicator_s)) {
      if (str_to_int.find(indicator_s) == str_to_int.end()) {
	str_to_int[indicator_s] = indicator_i;
	reference.group_names.emplace_back(indicator_s);
	reference.grouping.sizes.emplace_back(0);
	++indicator_i;
      }
      ++reference.grouping.sizes[str_to_int[indicator_s]];
      reference.grouping.indicators.emplace_back(str_to_int[indicator_s]);
    }
  } else {
    throw std::runtime_error("Could not read cluster indicators.");
  }

  reference.n_refs = reference.grouping.indicators.size();
  reference.grouping.n_groups = str_to_int.size();
}

void MatchClusterIndicators(const char delim, std::istream &groups, std::istream &fasta, Reference &reference) {
  std::unordered_map<std::string, unsigned> str_to_int;
  std::vector<std::string> groups_in_fasta;
  try {
    mSWEEP::tools::matchfasta(groups, fasta, delim, &groups_in_fasta);
  } catch (std::exception &e) {
    throw std::runtime_error("Matching the group indicators to the fasta file failed, is the --groups-list delimiter correct?");
  }

  unsigned indicator_i = 0;
  for (uint32_t i = 0; i < groups_in_fasta.size(); ++i) {
    if (str_to_int.find(groups_in_fasta[i]) == str_to_int.end()) {
      str_to_int[groups_in_fasta[i]] = indicator_i;
      reference.group_names.emplace_back(groups_in_fasta[i]);
      reference.grouping.sizes.emplace_back(0);
      ++indicator_i;
    }
    ++reference.grouping.sizes[str_to_int[groups_in_fasta[i]]];
    reference.grouping.indicators.emplace_back(str_to_int[groups_in_fasta[i]]);
  }

  reference.n_refs = reference.grouping.indicators.size();
  reference.grouping.n_groups = str_to_int.size();
}

std::vector<std::string> ReadCellNames(std::istream &cells_file) {
  Reference reference;
  ReadClusterIndicators(cells_file, reference);
  return reference.group_names;
}

void ReadBitfield(KallistoFiles &kallisto_files, unsigned n_refs, std::vector<std::unique_ptr<Sample>> &batch, Reference &reference, bool bootstrap_mode) {
  if (bootstrap_mode) {
    batch.emplace_back(new BootstrapSample());
  } else {
    batch.emplace_back(new Sample());
  }
  batch.back()->read_kallisto(n_refs, *kallisto_files.ec, *kallisto_files.tsv);
}

void ReadBitfield(const std::string &tinfile1, const std::string &tinfile2, const std::string &themisto_mode, const bool bootstrap_mode, const unsigned n_refs, std::vector<std::unique_ptr<Sample>> &batch) {
  std::vector<std::istream*> strands(2);
  File::In check_strand_1(tinfile1);
  File::In check_strand_2(tinfile2);
  strands.at(0) = new bxz::ifstream(tinfile1);
  strands.at(1) = new bxz::ifstream(tinfile2);

  if (bootstrap_mode) {
    batch.emplace_back(new BootstrapSample());
  } else {
    batch.emplace_back(new Sample());
  }

  batch.back()->read_themisto(get_mode(themisto_mode), n_refs, strands);
}
