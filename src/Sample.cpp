#include "Sample.hpp"

#include "likelihood.hpp"
#include "version.h"

void Sample::process_aln(const bool create_ids) {
  cell_id = "";
  m_num_ecs = aln.size();
  m_num_refs = aln.n_targets();

  log_ec_counts.resize(m_num_ecs, 0.0);
  if (create_ids) {
    aln.access_ec_ids()->resize(m_num_ecs, 0);
  }

  uint32_t aln_counts_total = 0;
#pragma omp parallel for schedule(static) reduction(+:aln_counts_total)
  for (uint32_t i = 0; i < m_num_ecs; ++i) {
    log_ec_counts[i] = std::log(aln.get_ec_counts()[i]);
    if (create_ids) {
      (*aln.access_ec_ids())[i] = i;
    }
    aln_counts_total += aln.get_ec_counts()[i];
  }
  clear_counts();
  clear_ids();
  counts_total = aln_counts_total;
}

std::vector<double> Sample::group_abundances() const {
  // Calculate the relative abundances of the
  // reference groups from the ec_probs matrix
  std::vector<double> thetas(this->ec_probs.get_rows(), 0.0);
  for (unsigned i = 0; i < this->ec_probs.get_rows(); ++i) {
    for (unsigned j = 0; j < this->ec_probs.get_cols(); ++j) {
      thetas[i] += std::exp(this->ec_probs(i, j) + this->log_ec_counts[j]);
    }
    thetas[i] /= this->counts_total;
  }
  return thetas;
}

std::vector<unsigned short> Sample::group_counts(const std::vector<unsigned short> indicators, const unsigned ec_id, const uint16_t n_groups) const {
  std::vector<unsigned short> read_hitcounts(n_groups);
  for (unsigned short j = 0; j < m_num_refs; ++j) {
    read_hitcounts[indicators[j]] += aln.get_ec_configs()[ec_id][j];
  }
  return read_hitcounts;
}

void SampleBS::init_bootstrap(Grouping &grouping) {
  ec_distribution = std::discrete_distribution<unsigned>(aln.get_ec_counts().begin(), aln.get_ec_counts().end());
  ll_mat = likelihood_array_mat(*this, grouping);
}

void SampleBS::resample_counts(std::mt19937_64 &generator) {
  std::vector<unsigned> tmp_counts(this->m_num_ecs);
  for (unsigned i = 0; i < this->counts_total; ++i) {
    unsigned ec_id = this->ec_distribution(generator);
    tmp_counts[ec_id] += 1;
  }
#pragma omp parallel for schedule(static)
  for (unsigned i = 0; i < this->m_num_ecs; ++i) {
    this->log_ec_counts[i] = std::log(tmp_counts[i]);
  }
}

void Sample::write_probabilities(const std::vector<std::string> &cluster_indicators_to_string, const bool gzip_probs, std::ostream &of) const {
  // Write the probability matrix to a file.
  if (of.good()) {
    of << "ec_id" << ',';
    for (unsigned i = 0; i < this->ec_probs.get_rows(); ++i) {
      of << cluster_indicators_to_string[i];
      of << (i < this->ec_probs.get_rows() - 1 ? ',' : '\n');
    }
    for (unsigned i = 0; i < this->ec_probs.get_cols(); ++i) {
      of << this->aln.get_ec_ids()[i] << ',';
      for (unsigned j = 0; j < this->ec_probs.get_rows(); ++j) {
	of << std::exp(this->ec_probs(j, i));
	of << (j < this->ec_probs.get_rows() - 1 ? ',' : '\n');
      }
    }
  }
  of << std::endl;
  of.flush();
}

void Sample::write_abundances(const std::vector<std::string> &cluster_indicators_to_string, std::string outfile) const {
  // Write relative abundances to a file,
  // outputs to std::cout if outfile is empty.
  const std::vector<double> &abundances = this->group_abundances();

  std::streambuf *buf;
  std::ofstream of;
  if (outfile.empty()) {
    buf = std::cout.rdbuf();
  } else {
    outfile += "_abundances.txt";
    of.open(outfile);
    buf = of.rdbuf();
  }
  std::ostream out(buf);
  out << "#mSWEEP_version:" << '\t' << _BUILD_VERSION << '\n';
  out << "#total_hits:" << '\t' << this->counts_total << '\n';
  out << "#c_id" << '\t' << "mean_theta" << '\n';
  for (size_t i = 0; i < abundances.size(); ++i) {
    out << cluster_indicators_to_string[i] << '\t' << abundances[i] << '\n';
  }
  if (!outfile.empty()) {
    of.close();
  }
}

void SampleBS::write_bootstrap(const std::vector<std::string> &cluster_indicators_to_string, std::string outfile, unsigned iters) {
  // Write relative abundances to a file,
  // outputs to std::cout if outfile is empty.
  std::streambuf *buf;
  std::ofstream of;
  if (outfile.empty()) {
    buf = std::cout.rdbuf();
  } else {
    outfile += "_abundances.txt";
    of.open(outfile);
    buf = of.rdbuf();
  }
  std::ostream out(buf);
  out << "#c_id" << '\t' << "mean_theta" << '\t' << "abundances" << '\t' << "bootstrap_abundances" << '\n';
  out << "#total_hits:" << '\t' << this->counts_total << '\n';

  for (size_t i = 0; i < cluster_indicators_to_string.size(); ++i) {
    out << cluster_indicators_to_string[i] << '\t';
    for (unsigned j = 0; j < iters; ++j) {
      out << this->bootstrap_abundances.at(j).at(i) << (j == iters - 1 ? '\n' : '\t');
    }
  }
  out << std::endl;
  if (!outfile.empty()) {
    of.close();
  }
}
