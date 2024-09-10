# Cebinae: Comparison with MAGIC

The original Cebinae README.md is [here](./README_Cebinae.md).
You don't have to read that file (feel free if you want to), below we list the necessary info.

### Build & Run Cebinae with NS-3.35
Cebinae simulation runs atop NS-3.35, an earlier NS version that uses waf build tool.
The NS3 and simulation code is in `ns/`, and Cebinae provides a `cebinae.py` python script in the home directory to help you run simulation.

You will need a Ubuntu 16/18/20/22.04 machine to run the Cebinae NS simulation.


* Validate environment set up.
    * `which python` points to `python3` or change the commands below to python3. Python version >= 3.8 required for packages like `scipy`.
    * `python cebinae.py ns validate -p optimized` to validate if the environment is well set up, which typically takes around 15 min. Upon failure of compilation or tests, check if the [prerequisite packages](https://www.nsnam.org/wiki/Installation#Ubuntu.2FDebian.2FMint) are installed for the corresponding platform, for instance, gcc >= 7.0.0. You can use `python cebinae.py ns prerequisite` auto-installs the set of packages needed.
    > Note: If you encounter compilation errors when compiling `ns/src/fd-net-device/model/dpdk-net-device.cc` that says a few DPDK macro like `ETH_LINK_DOWN` and `ETH_MQ_TX_NONE` are undefined, just substitute the macros to what the compiler suggests in that piece of code. This is because NS-3.35 uses many obsolete DPDK macros that are renamed in later DPDK C++ header files. Besides, there is no `split_hdr_size` member in `portConf.rxmode` (Line 325), just comment out that line if the compiler complains.

Each Cebinae comes with a config file located in `ns/configs`. We have three experiments regarding Cebinae with different config files.

1. Convergence speed experiment (MAGIC exp #1). Config file is `ns/configs/convergence-cebinae.json`. You can run this exp with command `python cebinae.py ns run_batch -c convergence-cebinae.json` in the home directory. It will find the target NS instance `convergence-cebinae` built from `ns/scratch/convergence-cebinae.cc` and output to `ns/tmp_index/convergence-cebinae/cebinae`.
2. FCT experiment on fat tree topo and Websearch dataset (MAGIC exp #5). Config file is `ns/configs/fat-tree-cebinae.json`. You can run this exp with command `python cebinae.py ns run_batch -c fat-tree-fct-cebinae.json` in the home directory. It will find the target NS instance `fat-tree-cebinae` built from `ns/scratch/fat-tree-cebinae.cc`, read the input topo and dataset from `ns/traces`, and output to `ns/tmp_index/fat-tree-cebinae/cebinae`.
3. Fairness experiment on multi-hop topo (MAGIC exp #6). Config file is `ns/configs/multi-hop-cebinae.json`. You can run this exp with command `python cebinae.py ns run_batch -c multi-hop-cebinae` in the home directory. It will find the target NS instance `multi-hop-cebinae` built from `ns/scratch/multi-hop-cebinae.cc` and output to `ns/tmp_index/multi-hop-cebinae/cebinae`.