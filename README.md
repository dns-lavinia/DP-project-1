# DP-project-1
For this project we chose to implement a peer-to-peer file transfer app with a centralizing server

## Compilation
To compile the code, run `make server` and `make peer`

## Running the program
### Starting the server
`./bin/server.exe`

### Starting a peer
Only for connecting to the server and become a seeder:
`./bin/peer.exe`

To connect to the server and request a file:
`./bin/peer.exe <file_name>`