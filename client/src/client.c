#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <sys/select.h>
#include <stdlib.h>
#include <arpa/inet.h>

void trim_spaces(char *str);
const char *extract_substr_in_brackets(const char *input);
int send_response(int tgtfd, char *dest_str);
int receive_response(int tgtfd, char *dest_str);
int store_file(int controlfd, int datafd, char *input, int pasv);
int retrieve_file(int controlfd, int datafd, char *input, int pasv);
int list_file(int controlfd, int datafd, int pasv);

int main(int argc, char **argv) {
	int sockfd, datafd;
	struct sockaddr_in addr, data_addr;
	char sentence[8192];
	int len;

	//创建socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//设置目标主机的ip和port
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[2]));
	if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {			//转换ip地址:点分十进制-->二进制
		printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//连接上目标主机（将socket和目标主机连接）-- 阻塞函数
	if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
		printf("Error connect(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

    // Get welcome message
    char initial_msg[100];
    memset(initial_msg, 0, sizeof(initial_msg));
    int initial_n = read(sockfd, initial_msg, 40);
    if (initial_n < 0) {
        printf("Error read(): %s(%d)\n", strerror(errno), errno);
		return 1;
    }
    initial_msg[initial_n] = '\0';
    printf("%s", initial_msg);

    char to_send[8192];

    while (1) {
        //获取键盘输入
        memset(sentence, 0, sizeof(sentence));
	    fgets(sentence, 4096, stdin);
	    len = strlen(sentence);
	    sentence[len - 1] = '\0';
        len = strlen(sentence);
        if (len == 0) continue;

        //Process according to the commands
        if (strncmp(sentence, "USER", 4) == 0) {
            char processed_username[100];
            snprintf(processed_username, sizeof(processed_username), "%s", sentence + 4);
            trim_spaces(processed_username);
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "USER %s", processed_username);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "PASS", 4) == 0) {
            char processed_email[200];
            snprintf(processed_email, sizeof(processed_email), "%s", sentence + 4);
            trim_spaces(processed_email);
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "PASS %s", processed_email);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "QUIT", 4) == 0) {
            memset(to_send, 0, sizeof(to_send));
            strcpy(to_send, "QUIT");

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else break;
        }
        else if (strncmp(sentence, "SYST", 4) == 0) {
            memset(to_send, 0, sizeof(to_send));
            strcpy(to_send, "SYST");

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "TYPE", 4) == 0) {
            char processed_type[100];
            snprintf(processed_type, sizeof(processed_type), "%s", sentence + 4);
            trim_spaces(processed_type);
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "TYPE %s", processed_type);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "MKD", 3) == 0) {
            char processed_path[500];
            snprintf(processed_path, sizeof(processed_path), "%s", sentence + 3);
            trim_spaces(processed_path);
            int len = strlen(processed_path);
            if (len > 0) {
                if (processed_path[len - 1] == '/') processed_path[len - 1] = '\0';
            }
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "MKD %s", processed_path);

            int n = write(sockfd, to_send, strlen(to_send));
            if (n < 0) {
                printf("Error write(): %s(%d)\n", strerror(errno), errno);
                return 1;
            }

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "RMD", 3) == 0) {
            char processed_path[500];
            snprintf(processed_path, sizeof(processed_path), "%s", sentence + 3);
            trim_spaces(processed_path);
            int len = strlen(processed_path);
            if (len > 0) {
                if (processed_path[len - 1] == '/') processed_path[len - 1] = '\0';
            }
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "RMD %s", processed_path);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "PWD", 3) == 0) {
            memset(to_send, 0, sizeof(to_send));
            strcpy(to_send, "PWD");

            int n = write(sockfd, to_send, strlen(to_send));
            if (n < 0) {
                printf("Error write(): %s(%d)\n", strerror(errno), errno);
		        return 1;
            }

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "CWD", 3) == 0) {
            char processed_path[500];
            snprintf(processed_path, sizeof(processed_path), "%s", sentence + 3);
            trim_spaces(processed_path);
            int len = strlen(processed_path);
            if (len > 0) {
                if (processed_path[len - 1] == '/') processed_path[len - 1] = '\0';
            }    
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "CWD %s", processed_path);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "RNFR", 4) == 0) {
            char processed_name[100];
            snprintf(processed_name, sizeof(processed_name), "%s", sentence + 4);
            trim_spaces(processed_name);
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "RNFR %s", processed_name);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "RNTO", 4) == 0) {
            char processed_name[100];
            snprintf(processed_name, sizeof(processed_name), "%s", sentence + 4);
            trim_spaces(processed_name);
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "RNTO %s", processed_name);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            else continue;
        }
        else if (strncmp(sentence, "PORT", 4) == 0) {
            char processed_input[200];
            snprintf(processed_input, sizeof(processed_input), "%s", sentence + 4);
            trim_spaces(processed_input);
            memset(to_send, 0, sizeof(to_send));
            snprintf(to_send, sizeof(to_send), "PORT %s", processed_input);

            char *token;
            char port_ip[20];
            int port_port;
            token = strtok(processed_input, ",");
            char port_ip_addr[50] = "";
            int count = 0;
            while ((token != NULL) && (++count < 5)) {
                strcat(port_ip_addr, ".");
                strcat(port_ip_addr, token);
                token = strtok(NULL, ",");
            }
            int h1 = atoi(token);
            token = strtok(NULL, ",");
            int h2 = atoi(token);
            port_port = (h1 << 8) + h2; 
            strcpy(port_ip, port_ip_addr + 1);

            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;
            
            if (strncmp(to_send, "200", 3) == 0) {
                if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                    printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                    return 1;
                }

                data_addr.sin_family = AF_INET;
                data_addr.sin_port = htons(port_port);
                if (inet_pton(AF_INET, port_ip, &data_addr.sin_addr) <= 0) {			//转换ip地址:点分十进制-->二进制
                    printf("ip: %s\n", port_ip);
                    printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
                    return 1;
                }

                if (bind(datafd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
                    printf("Error binding data socket");
                    exit(1);
                }

                if (listen(datafd, 10) == -1) {
                    printf("Error listen(): %s(%d)\n", strerror(errno), errno);
                    return 1;
                }

                struct sockaddr_in client_addr;
                socklen_t client_addr_len = sizeof(client_addr);

                memset(sentence, 0, sizeof(sentence));
                fgets(sentence, 4096, stdin);
                len = strlen(sentence);
                sentence[len - 1] = '\0';
                len = strlen(sentence);
                if (len == 0) continue;

                if (strncmp(sentence, "STOR", 4) == 0) {
                    char filename[256];
                    strcpy(filename, sentence + 4);
                    trim_spaces(filename);
                    memset(to_send, 0, sizeof(to_send));
                    snprintf(to_send, sizeof(to_send), "STOR %s", filename);
                    int sen = send_response(sockfd, to_send);
                    if (sen == 1) return 1;

                    // recv 150
                    int res = receive_response(sockfd, to_send);
                    if (res == 1) return 1;
                    else if (res == 2) break;

                    while (1) {
                        //if ((datafd = accept(datafd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
                        if ((datafd = accept(datafd, NULL, NULL)) == -1) {
                            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                            continue;
                        }
                        store_file(sockfd, datafd, filename, 0);
                        break;
                    }   
                }
                else if (strncmp(sentence, "RETR", 4) == 0) {
                    char filename[256];
                    strcpy(filename, sentence + 4);
                    trim_spaces(filename);
                    memset(to_send, 0, sizeof(to_send));
                    snprintf(to_send, sizeof(to_send), "RETR %s", filename);
                    int sen = send_response(sockfd, to_send);
                    if (sen == 1) return 1;

                    // recv 150
                    int res = receive_response(sockfd, to_send);
                    if (res == 1) return 1;
                    else if (res == 2) break;

                    while (1) {
                        if ((datafd = accept(datafd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
                            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                            continue;
                        }
                        retrieve_file(sockfd, datafd, filename, 0);
                        break;
                    }   
                }
                else if (strncmp(sentence, "LIST", 4) == 0) {
                    memset(to_send, 0, sizeof(to_send));
                    strcpy(to_send, "LIST");
                    int sen = send_response(sockfd, to_send);
                    if (sen == 1) return 1;

                    // recv 150
                    int res = receive_response(sockfd, to_send);
                    if (res == 1) return 1;
                    else if (res == 2) break;

                    while (1) {
                        if ((datafd = accept(datafd, (struct sockaddr*)&client_addr, &client_addr_len)) == -1) {
                            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
                            continue;
                        }
                        list_file(sockfd, datafd, 0);
                        break;
                    }   
                }

                close(datafd);
            }

        }
        else if(strncmp(sentence, "PASV", 4) == 0) {
            memset(to_send, 0, sizeof(to_send));
            strcpy(to_send, "PASV");
        
            int sen = send_response(sockfd, to_send);
            if (sen == 1) return 1;

            int res = receive_response(sockfd, to_send);
            if (res == 1) return 1;
            else if (res == 2) break;

            if (strncmp(to_send, "227", 3) == 0) {
                // Extract server ip and port
                int data_port;
                char data_ip[50];
                char extract_sentence[50];
                strcpy(extract_sentence, extract_substr_in_brackets(to_send));
                char *token;
                token = strtok(extract_sentence, ",");
                char port_ip_addr[50] = "";
                int count = 0;
                while ((token != NULL) && (++count < 5)) {
                    strcat(port_ip_addr, ".");
                    strcat(port_ip_addr, token);
                    token = strtok(NULL, ",");
                }
                int h1 = atoi(token);
                token = strtok(NULL, ",");
                int h2 = atoi(token);
                data_port = (h1 << 8) + h2; 
                strcpy(data_ip, port_ip_addr + 1);

                if ((datafd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
                    printf("Error socket(): %s(%d)\n", strerror(errno), errno);
                    return 1;
                }

                data_addr.sin_family = AF_INET;
                data_addr.sin_port = htons(data_port);
                if (inet_pton(AF_INET, data_ip, &(data_addr.sin_addr)) <= 0) {
                    perror("Error converting data IP address");
                    close(datafd);
                    close(sockfd);
                    exit(1);
                }

                if (connect(datafd, (struct sockaddr*)&data_addr, sizeof(data_addr)) < 0) {
                    printf("Error connect(): %s(%d)\n", strerror(errno), errno);
                    close(datafd);
                    close(sockfd);
                    return 1;
                }

                // recv 150
                int res = receive_response(sockfd, to_send);
                if (res == 1) return 1;
                else if (res == 2) break;

                memset(sentence, 0, sizeof(sentence));
                fgets(sentence, 4096, stdin);
                len = strlen(sentence);
                sentence[len - 1] = '\0';
                len = strlen(sentence);
                if (len == 0) continue;

                if (strncmp(sentence, "STOR", 4) == 0) {
                    char filename[256];
                    strcpy(filename, sentence + 4);
                    trim_spaces(filename);
                    store_file(sockfd, datafd, filename, 1);
                }
                else if (strncmp(sentence, "RETR", 4) == 0) {
                    char filename[256];
                    strcpy(filename, sentence + 4);
                    trim_spaces(filename);
                    retrieve_file(sockfd, datafd, filename, 1);
                }
                else if (strncmp(sentence, "LIST", 4) == 0) {
                    list_file(sockfd, datafd, 1);
                }

                close(datafd);
            }
        }
        else {
            memset(to_send, 0, sizeof(to_send));
            strcpy(to_send, "error");

            int n = write(sockfd, to_send, strlen(to_send));
            if (n < 0) {
                printf("Error write(): %s(%d)\n", strerror(errno), errno);
		        return 1;
            }

            //榨干socket接收到的内容
            memset(to_send, 0, sizeof(to_send));
            n = read(sockfd, to_send, sizeof(to_send));
		    if (n < 0) {
			    printf("Error read(): %s(%d)\n", strerror(errno), errno);	
    			return 1;
	    	} 
            else if (n == 0) {
                printf("connection closed by the server\n");
                break;
            }
		    else {
                to_send[n] = '\0';
                printf("%s", to_send);
            }
        }
	}
    
	close(sockfd);

	return 0;
}

// ======================================================================
// Remove spaces at the start & end of string
void trim_spaces(char *str) {
    int start = 0, end = strlen(str) - 1;
    while (str[start] == ' ' || str[start] == '\t' || str[start] == '\n') {
        start++;
    }
    while (end > start && (str[end] == ' ' || str[end] == '\t' || str[end] == '\n')) {
        end--;
    }
    if (end < start) {
        str[0] = '\0';
    } 
    else {
        int i, j = 0;
        for (i = start; i <= end; i++) {
            str[j++] = str[i];
        }
        str[j] = '\0'; 
    }
}

// Extract contents in brackets
const char *extract_substr_in_brackets(const char *input) {
    const char *start = strchr(input, '(');
    if (start) {
        start++;  
        const char *end = strchr(start, ')');
        if (end) {
            size_t length = end - start;
            
            char *substring = (char *)malloc(length + 1);
            if (substring) {
                strncpy(substring, start, length);
                substring[length] = '\0'; 
                return substring;
            }
        }
    }
    return NULL;  
}

// Send response from socket
int send_response(int tgtfd, char *dest_str) {
    int p = 0;
    int len = strlen(dest_str);
	while (p < len) {
		int n = write(tgtfd, dest_str + p, len + 1 - p);		//write函数不保证所有的数据写完，可能中途退出
		if (n < 0) {
			printf("Error write(): %s(%d)\n", strerror(errno), errno);
			return 1;
 		} else {
			p += n;
		}			
	}
    return 2;
}

// Reveice response from socket & print
int receive_response(int tgtfd, char *dest_str) {
    memset(dest_str, 0, strlen(dest_str));

    int p = 0;
	while (1) {
		int n = read(tgtfd, dest_str + p, 8191 - p);
		if (n < 0) {
			printf("Error read(): %s(%d)\n", strerror(errno), errno);	//read不保证一次读完，可能中途退出
			return 1;
		} else if (n == 0) {
            printf("Connection closed by the server.\n");
			return 2;
		} else {
			p += n;
			if (dest_str[p - 1] == '\n') {
				break;
			}
		}
	}
    //注意：read并不会将字符串加上'\0'，需要手动添加
	dest_str[p] = '\0';
    printf("%s", dest_str);
    return 3;
}

// Store file from current directory
int store_file(int controlfd, int datafd, char *input, int pasv) {
    char filename[256], str[8193], recvline[8193], sendline[8193], temp1[1024];
    memset(filename, 0, sizeof(filename));
    memset(recvline, 0, sizeof(recvline));
    memset(str, 0, sizeof(str));

    fd_set wrset, rdset;
    int maxfdp1, data_finished = 0, control_finished = 0;

    strcpy(filename, input);
    sprintf(str, "STOR %s", filename);
    sprintf(temp1, "cat %s", filename);
    memset(filename, 0, sizeof(filename));

    FD_ZERO(&wrset);
    FD_ZERO(&rdset);
    FD_SET(controlfd, &rdset);
    FD_SET(datafd, &wrset);

    if (controlfd > datafd) maxfdp1 = controlfd + 1;
    else maxfdp1 = datafd + 1;

    if (pasv) write(controlfd, str, strlen(str));

    // Read video data from the file
    FILE *videoFile = fopen(input, "rb");
    if (videoFile == NULL) {
        perror("fopen");
        return -1;  // Handle the error appropriately
    }

    size_t bytesRead;

    while(1) {
        if (!control_finished) FD_SET(controlfd, &rdset);
        if (!data_finished) FD_SET(datafd, &wrset);
        select(maxfdp1, &rdset, &wrset, NULL, NULL);

        if (FD_ISSET(controlfd, &rdset)) {
            memset(recvline, 0, sizeof(recvline));
            read(controlfd, recvline, sizeof(recvline));
            printf("%s", recvline);
            control_finished = 1;
            memset(recvline, 0, sizeof(recvline));
            FD_CLR(controlfd, &rdset);
        }

        if (FD_ISSET(datafd, &wrset)) {
            memset(sendline, 0, sizeof(sendline));
            while ((bytesRead = fread(sendline, 1, sizeof(sendline), videoFile)) > 0) {
                write(datafd, sendline, bytesRead);
                memset(sendline, 0, sizeof(sendline));
            }

            data_finished = 1;
            FD_CLR(datafd, &wrset);
            close(datafd);
        }

        if (control_finished && data_finished) break;
    }

    fclose(videoFile);
    return 1;
}

// List content in current working directory od server
int list_file(int controlfd, int datafd, int pasv) {
    char str[8193], recvline[8193];
    memset(recvline, 0, sizeof(recvline));
    memset(str, 0, sizeof(str));

    fd_set rdset;
    int maxfdp1, data_finished = 0, control_finished = 0;

    sprintf(str, "LIST");

    FD_ZERO(&rdset);
    FD_SET(controlfd, &rdset);
    FD_SET(datafd, &rdset);

    if (controlfd > datafd) {
        maxfdp1 = controlfd + 1;
    }
    else {
        maxfdp1 = datafd + 1;
    }

    if (pasv) write(controlfd, str, strlen(str));
    while (1) {
        if (!control_finished ) FD_SET(controlfd, &rdset);
        if (!data_finished) FD_SET(datafd, &rdset);
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if (FD_ISSET(controlfd, &rdset)) {
            read(controlfd, recvline, sizeof(recvline));
            printf("%s", recvline);
            control_finished = 1;
            memset(recvline, 0, sizeof(recvline));
            FD_CLR(controlfd, &rdset);
        }

        if (FD_ISSET(datafd, &rdset)) {
            while(read(datafd, recvline, sizeof(recvline)) > 0){
                printf("%s", recvline); 
                memset(recvline, 0, sizeof(recvline));
            }

            data_finished = 1;
            FD_CLR(datafd, &rdset);
        }

        if (control_finished && data_finished) break;
    }
    memset(recvline, 0, sizeof(recvline));
    memset(str, 0, sizeof(str));

    return 1;
}

int retrieve_file(int controlfd, int datafd, char *input, int pasv) {
    char filename[256], str[8193], recvline[8193], temp1[1024];
    memset(filename, 0, sizeof(filename));
    memset(recvline, 0, sizeof(recvline));
    memset(str, 0, sizeof(str));
    int n = 0;

    fd_set rdset;
    int maxfdp1, data_finished = 0, control_finished = 0;

    strcpy(filename, input);
    sprintf(str, "RETR %s", filename);
    sprintf(temp1, "%s", filename);
    memset(filename, 0, sizeof(filename));

    FILE *fp;
    if ((fp = fopen(temp1, "wb")) == NULL) {
        perror("file error");
        return -1;
    }

    if (pasv) write(controlfd, str, strlen(str));

    FD_ZERO(&rdset);
    FD_SET(controlfd, &rdset);
    FD_SET(datafd, &rdset);

    if (controlfd > datafd) maxfdp1 = controlfd + 1;
    else maxfdp1 = datafd + 1;

    while (1) {
        if (!control_finished) FD_SET(controlfd, &rdset);
        if (!data_finished) FD_SET(datafd, &rdset);
        select(maxfdp1, &rdset, NULL, NULL, NULL);

        if (FD_ISSET(controlfd, &rdset)) {
            memset(recvline, 0, sizeof(recvline));
            read(controlfd, recvline, sizeof(recvline));
            printf("%s", recvline);
            control_finished = 1;
            memset(recvline, 0, sizeof(recvline));
            FD_CLR(controlfd, &rdset);
        }

        if (FD_ISSET(datafd, &rdset)) {
            memset(recvline, 0, sizeof(recvline));
            while ((n = read(datafd, recvline, sizeof(recvline))) > 0) {
                fwrite(recvline, 1, n, fp);
                memset(recvline, 0, sizeof(recvline));
            }

            if (n < 0) {
                perror("read datafd");
                fclose(fp);
                return -1;
            }

            if (n == 0) {
                data_finished = 1;
                FD_CLR(datafd, &rdset);
            }
        }

        if (control_finished && data_finished) break;
    }

    fclose(fp);
    return 1;
}
