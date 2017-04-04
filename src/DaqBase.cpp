#include <cstdio>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <stdexcept>

#include "check_macros.h"
#include "DaqBase.h"


DaqBase::DaqBase(int argc, char *argv[], const char *process) : m_process(process) {
  if (argc != 3) {
    std::cerr << "Usage: " << process << " takes 2 arguments: "<< std::endl;
    std::cerr << "       config.yaml"<< std::endl;
    std::cerr << "       id within process"<< std::endl;
    throw std::runtime_error("Invalid command line arguments");
  }

  m_config = YAML::LoadFile(argv[1]);
  m_process_config = m_config[process];
  m_id = atoi(argv[2]);

  m_basename = form_basename(m_process, m_id);
  m_fname_h5 = form_fullpath(m_process, m_id, HDF5);
  m_fname_pid = form_fullpath(m_process, m_id, PID);
  m_fname_finished = form_fullpath(m_process, m_id, FINISHED);

  // will set to "small" -> ["fiducials", "nano", "data"] ...
  m_group2dsets = get_top_group_to_final_dsets();

}


std::string DaqBase::form_basename(std::string process, int idx) {
  static char idstr[128];
  sprintf(idstr, "%4.4d", idx);
  return process + "-s" + idstr;
}


std::string DaqBase::form_fullpath(std::string process, int idx, enum Location location) {
  std::string basename = form_basename(process, idx);
  std::string full_path = m_config["rootdir"].as<std::string>() + 
    "/" +
    m_config["rundir"].as<std::string>();
  switch (location) {
  case HDF5:
    full_path += "/hdf5/" + basename + ".h5";
    break;
  case PID:
    full_path += "/pids/" + basename + ".pid";
    break;
  case LOG:
    full_path += "/logs/" + basename + ".log";
    break;
  case FINISHED:
    full_path += "/logs/" + basename + ".finished";
    break;
  }
  return full_path;
}


void DaqBase::run_setup() {
  std::chrono::time_point<std::chrono::system_clock> start_run, end_run;
  start_run = std::chrono::system_clock::now();
  std::time_t start_run_time = std::chrono::system_clock::to_time_t(start_run);
  m_t0 = Clock::now();
    
  std::cout << m_basename << ": start_time: " << std::ctime(&start_run_time) << std::endl;
}


void DaqBase::write_pid_file() {
  pid_t pid = ::getpid();
  char hostname[256];
  if (0 != gethostname(hostname, 256)) {
    sprintf(hostname, "--unknown--");
    std::cerr << "DaqWriter: gethostname failed in write_pid_file" << std::endl;
  }

  FILE *pid_f = ::fopen(m_fname_pid.c_str(), "w");
  if (NULL == pid_f) {
    std::cerr << "Could not create file: " << m_fname_pid << std::endl;
    throw std::runtime_error("FATAL - write_pid_file");
  }
  fprintf(pid_f, "process=%s idx=%d hostname=%s pid=%d\n", 
          m_process.c_str(), m_id, hostname, pid);
  fclose(pid_f);
}


void DaqBase::create_standard_groups(hid_t parent) {
  m_small_group = NONNEG( H5Gcreate2(parent, "small", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );  
  m_vlen_group = NONNEG( H5Gcreate2(parent, "vlen", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );  
  m_cspad_group = NONNEG( H5Gcreate2(parent, "cspad", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );  
}


void DaqBase::create_number_groups(hid_t parent, TSubMap &sub_map, int first, int count) {
  for (int name = first; name < (first + count); ++name) {
    char strname[128];
    sprintf(strname, "%5.5d", name);
    hid_t dset_group = NONNEG( H5Gcreate2(parent, strname, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT) );
    sub_map[name]=dset_group;
  } 
}


void DaqBase::close_number_groups(std::map<int, hid_t> &name_to_group) {
  std::map<int, hid_t>::iterator pos;
  for (pos = name_to_group.begin(); pos != name_to_group.end(); ++pos) {
    NONNEG( H5Gclose( pos->second ) );
  }
  name_to_group.clear();
}


void DaqBase::close_standard_groups() {
  NONNEG( H5Gclose( m_cspad_group ) );
  NONNEG( H5Gclose( m_vlen_group ) );
  NONNEG( H5Gclose( m_small_group ) );
}
  

void DaqBase::load_cspad(const std::string &h5_filename,
                         const std::string &dataset,
                         int length,
                         std::vector<short> &cspad_buffer) {
  size_t total = size_t(CSPadNumElem) * size_t(length);
  cspad_buffer.resize(total);
  for (size_t jj = 0; jj < size_t(length); ++jj) {
    short val = short(jj / CSPadNumElem);
    cspad_buffer.at(jj) = val;
  }
}

DaqBase::~DaqBase() {
  FILE *finished_f = fopen(m_fname_finished.c_str(), "w");
  if (NULL==finished_f) {
    std::cerr << "could not create finished file\n" << std::endl;
    return;
  }
  fprintf(finished_f,"done.\n");
  fclose(finished_f);
}