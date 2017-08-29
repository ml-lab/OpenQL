/**
 * @file   program.h
 * @date   04/2017
 * @author Nader Khammassi
 *         Imran Ashraf
 * @brief  openql program
 */

#ifndef QL_PROGRAM_H
#define QL_PROGRAM_H

#include <ql/utils.h>
#include <ql/platform.h>
#include <ql/kernel.h>
#include <ql/interactionMatrix.h>
#include <ql/eqasm_compiler.h>
#include <ql/arch/cbox_eqasm_compiler.h>
#include <ql/arch/cc_light_eqasm_compiler.h>

namespace ql
{

extern bool initialized;

/**
 * quantum_program_
 */
class quantum_program
{

   public: 

      std::string           eqasm_compiler_name;
      ql::eqasm_compiler *  backend_compiler;
      ql::quantum_platform  platform; 

   public:

      quantum_program(std::string name, size_t nqubits /**/, quantum_platform platform) : platform(platform), name(name), default_config(true), qubits(nqubits)
      {
         eqasm_compiler_name = platform.eqasm_compiler_name;
	 backend_compiler    = NULL;
         if (eqasm_compiler_name =="")
         {
            println("[x] error : eqasm compiler name must be specified in the hardware configuration file !");
            throw std::exception();
         }
         else if (eqasm_compiler_name == "qumis_compiler")
         {
            backend_compiler = new ql::arch::cbox_eqasm_compiler();
         }
         else if (eqasm_compiler_name == "none")
         {

         }
	 else if (eqasm_compiler_name == "cc_light_compiler" )
	 {
            backend_compiler = new ql::arch::cc_light_eqasm_compiler();
	 }
         else
         {
            println("[x] error : the '" << eqasm_compiler_name << "' eqasm compiler backend is not suported !");
            throw std::exception();
         }
      }

      void add(ql::quantum_kernel k)
      {
         kernels.push_back(k);
      }

      void set_config_file(std::string file_name)
      {
         config_file_name = file_name;
         default_config   = false;
      }

      std::string qasm()
      {
         std::stringstream ss;
         ss << "# this file has been automatically generated by the OpenQL compiler please do not modify it manually.\n";
         ss << "qubits " << qubits << "\n";
         for (size_t k=0; k<kernels.size(); ++k)
            ss <<'\n' << kernels[k].qasm();
	/*
         ss << ".cal0_1\n";
         ss << "   prepz q0\n";
         ss << "   measure q0\n";
         ss << ".cal0_2\n";
         ss << "   prepz q0\n";
         ss << "   measure q0\n";
         ss << ".cal1_1\n";
         ss << "   prepz q0\n";
         ss << "   x q0\n";
         ss << "   measure q0\n";
         ss << ".cal1_2\n";
         ss << "   prepz q0\n";
         ss << "   x q0\n";
         ss << "   measure q0\n";
	 */
         return ss.str();
      }

      std::string microcode()
      {
         std::stringstream ss;
         ss << "# this file has been automatically generated by the OpenQL compiler please do not modify it manually.\n";
         ss << uc_header();
         for (size_t k=0; k<kernels.size(); ++k)
            ss <<'\n' << kernels[k].micro_code();
	 /*
         ss << "     # calibration points :\n";  // wait for 100us
         // calibration points for |0>
         for (size_t i=0; i<2; ++i)
         {
            ss << "     waitreg r0               # prepz q0 (+100us) \n";  // wait for 100us
            ss << "     waitreg r0               # prepz q0 (+100us) \n";  // wait for 100us
            // measure
            ss << "     wait 60 \n";  // wait
            ss << "     pulse 0000 1111 1111 \n";  // pulse
            ss << "     wait 50\n";
            ss << "     measure\n";  // measurement discrimination
         }

         // calibration points for |1>
         for (size_t i=0; i<2; ++i)
         {
            // prepz
            ss << "     waitreg r0               # prepz q0 (+100us) \n";  // wait for 100us
            ss << "     waitreg r0               # prepz q0 (+100us) \n";  // wait for 100us
            // X
            ss << "     pulse 1001 0000 1001     # X180 \n";
            ss << "     wait 10\n";
            // measure
            ss << "     wait 60\n";  // wait
            ss << "     pulse 0000 1111 1111\n";  // pulse
            ss << "     wait 50\n";
            ss << "     measure\n";  // measurement discrimination
         }
	 */

         ss << "     beq  r3,  r3, loop   # infinite loop";
         return ss.str();
      }

      void set_platform(quantum_platform platform)
      {
         this->platform = platform;
      }

      std::string uc_header()
      {
         std::stringstream ss;
         ss << "# auto-generated micro code from rb.qasm by OpenQL driver, please don't modify it manually \n";
         ss << "mov r11, 0       # counter\n";
         ss << "mov r3,  10      # max iterations\n";
         ss << "mov r0,  20000   # relaxation time / 2\n";
         // ss << name << ":\n";
         ss << "loop:\n";
         return ss.str();
      }

      int compile(bool ql_optimize=false, bool verbose=false) throw (ql::exception)
      {
         // if (!ql::initialized)
         // {
         //    println("[x] error : openql should initialized for the target platform before compilation !");
         //    return -1;
         // }
         if (verbose) println("compiling ...");
         if (kernels.empty())
            return -1;

         if(ql_optimize)
         {
            if (verbose)
            {
               println("optimizations enabled");
               println("optimizing quantum kernels...");
            }
            for (size_t k=0; k<kernels.size(); ++k)
               kernels[k].optimize();
         }

         std::stringstream ss_qasm;
         ss_qasm << ql::utils::get_output_dir() << "/" << name << ".qasm";
         std::string s = qasm();

         if (verbose) println("writing qasm to '" << ss_qasm.str() << "' ...");
         ql::utils::write_file(ss_qasm.str(),s);

         if (backend_compiler == NULL)
         {
            println("warning : no eqasm compiler has been specified in the configuration file, only qasm code has been compiled.");
            return 0;
         }

         // println("sweep_points : ");
         // for (int i=0; i<sweep_points.size(); i++) println(sweep_points[i]);

         if (verbose)
            println("fusing quantum kernels...");

         ql::circuit fused;

         for (size_t k=0; k<kernels.size(); ++k)
         {
            ql::circuit& kc = kernels[k].get_circuit();
            fused.insert(fused.end(),kc.begin(),kc.end());
         }

	 try 
	 {
	    println("compiling eqasm code...");
	    backend_compiler->compile(fused, platform);
	 }
	 catch (ql::exception e)
	 {
	    println("[x] error : eqasm_compiler.compile() : compilation interrupted due to fatal error.");
	    throw e;
	 }

         println("writing eqasm code to '" << ( ql::utils::get_output_dir() + "/" + name+".asm") << "'...");
         backend_compiler->write_eqasm( ql::utils::get_output_dir() + "/" + name + ".asm");
         println("writing traces...");
         backend_compiler->write_traces( ql::utils::get_output_dir() + "/trace.dat");

         // deprecated hardcoded microcode generation 

         /*
         std::stringstream ss_asm;
         ss_asm << output_path << name << ".asm";
         std::string uc = microcode();

         if (verbose) println("writing transmon micro-code to '" << ss_asm.str() << "' ...");
         ql::utils::write_file(ss_asm.str(),uc);
         */

	 if (sweep_points.size())
	 {
         std::stringstream ss_swpts;
         ss_swpts << "{ \"measurement_points\" : [";
         for (size_t i=0; i<sweep_points.size()-1; i++)
            ss_swpts << sweep_points[i] << ", ";
         ss_swpts << sweep_points[sweep_points.size()-1] << "] }";
         std::string config = ss_swpts.str();
         if (default_config)
         {
            std::stringstream ss_config;
            ss_config << ql::utils::get_output_dir() << "/" << name << "_config.json";
            std::string conf_file_name = ss_config.str();
            if (verbose) println("writing sweep points to '" << conf_file_name << "'...");
            ql::utils::write_file(conf_file_name, config);
         }
         else
         {
            std::stringstream ss_config;
            ss_config << ql::utils::get_output_dir() << "/" << config_file_name;
            std::string conf_file_name = ss_config.str();
            if (verbose) println("writing sweep points to '" << conf_file_name << "'...");
            ql::utils::write_file(conf_file_name, config);
         }
	 }
	 else
	 {
	    println("[x] error : cannot write sweepoint file : sweep point array is empty !");
	 }

	 println("compilation of program '" << name << "' done.");

         return 0;
      }

      void schedule(std::string scheduler="ASAP", bool verbose=false)
      {
         std::string sched_qasm;

         if (verbose)
            println("scheduling the quantum program");

         for (auto k : kernels)
         {
            std::string kernel_sched_qasm;
            std::string kernel_sched_dot;
            k.schedule(qubits, platform, scheduler, kernel_sched_qasm, kernel_sched_dot, verbose);
            sched_qasm += "." + k.getName() + "\n";
            sched_qasm += kernel_sched_qasm;

            string fname = ql::utils::get_output_dir() + "/" + k.getName() + scheduler + ".dot";
            if (verbose) println("writing scheduled qasm to '" << fname << "' ...");
            ql::utils::write_file(fname, kernel_sched_dot);
         }

         string fname = ql::utils::get_output_dir() + "/" + name + scheduler + ".qasm";
         if (verbose) println("writing scheduled qasm to '" << fname << "' ...");
         ql::utils::write_file(fname, sched_qasm);
      }

      void print_interaction_matrix(bool verbose=false)
      {
         if (verbose)
            println("printing interaction matrix...");

         for (auto k : kernels)
         {
            InteractionMatrix imat( k.getCircuit(), qubits);
            string mstr = imat.getString();
            std::cout << mstr << std::endl;
         }
      }

      void write_interaction_matrix(bool verbose=false)
      {
         for (auto k : kernels)
         {
            InteractionMatrix imat( k.getCircuit(), qubits);
            string mstr = imat.getString();

            string fname = ql::utils::get_output_dir() + "/" + k.getName() + "InteractionMatrix.dat";
            if (verbose) println("writing interaction matrix to '" << fname << "' ...");
            ql::utils::write_file(fname, mstr);
         }
      }

      void set_sweep_points(float * swpts, size_t size)
      {
         sweep_points.clear();
         for (size_t i=0; i<size; ++i)
            sweep_points.push_back(swpts[i]);
      }

   protected:

      // ql_platform_t               platform;
      std::vector<quantum_kernel> kernels;
      std::vector<float>          sweep_points;
      std::string                 name;
      // std::string                 output_path;
      std::string                 config_file_name;
      bool                        default_config;
      size_t                      qubits;
};

} // ql

#endif //QL_PROGRAM_H
