<div align="center" style="text-align: center; width: 100%">
<h1>P2P file sharing system<br/><sub>Dynamic local network file sharing network</sub></h1>
<br/>
</div>

A C based file sharing system meant to run on Linux based containers connected via a local network, partitioning stored files among the node network. Based on the `Chord DHT algorithm` it supports dynamic changes to the network of connected containers, in addition to implementing data redundancy in case of unexpected node failures. 

# Usage
The `main` main executable contains the logic for the entire program, each node must additionally be setup with the following elements in the root directory of where `main` will be run: 
- `nodeInfo` directory
- `shared` directory

Does not support file creation/editing within program, all files uploaded to the network must previously be found within the container's file system. 
> To start using the system, run the following in the command line `./main`
