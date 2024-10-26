gcc -o chat_server chat_server.c -lrt
gcc -o chat_client chat_client.c -lrt


./chat_server
./chat_client client1
./chat_client client2
