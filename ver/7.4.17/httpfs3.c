

#define FUSE_USE_VERSION 30
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>
#include <curl/curl.h>
#include <regex.h>


#define NAME_MAX 255
#define SERVER_URL broskini.xf.cz/folder1/
#define QUOTE(name) #name
#define STR(macro) QUOTE(macro)

//pomocna struktura
struct SUBOR {
	
	char fname[NAME_MAX];
	char date[NAME_MAX];
	char time[NAME_MAX];
	char size[NAME_MAX];
	char type[NAME_MAX];
	struct SUBOR *next;
	
	};

int counter = 0; //pomocne pocitadlo


//typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *sbuft, off_t off);

//struktura pre polozky adresara
struct entry{	
	char name[NAME_MAX];
	struct entry *next;	
	};
	
//struktura pre uchovanie http odpovede
struct string {
	char *ptr;  //obsahuje samotny obsah http response servera
	size_t len; //dlzka retazca
	};
	

//funkcia pre inicializovanie struktury stringu
void init_str(struct string *str) {
	str->len = 0;
	str->ptr = malloc(str->len + 1);
	if(str->ptr == NULL) { printf("error\n"); }
	str->ptr[0]='\0';

}

//funkcia ktora zapise http obsah do struct string
size_t writef(void *ptr, size_t size, size_t nmemb, struct string *str) {
	size_t len1 = str->len + size*nmemb;
	str->ptr = realloc(str->ptr, len1+1);
	if(str->ptr == NULL) { printf("error\n");}
	
	memcpy(str->ptr+str->len, ptr, size*nmemb);
	str->ptr[len1] = '\0';
	str->len = len1;
	
	return size*nmemb;
} 

//funkcia sluziaca na uvolnenie spajaneho zoznamu
void free_list(struct entry *head) {
	
	struct entry *pom;
	
	while(head != NULL) {
		pom = head;
		head = head->next;
		free(pom);
		}
	
	}
	
//pomocna funkcia pre uvolnenie pomocneho zoznamu
void free_list2(struct SUBOR *head) {
	
	struct SUBOR *pom;
	
	while(head != NULL) {
		pom = head;
		head = head->next;
		free(pom);
		}
	
	}


//funkcia, ktora vykona http spojenie a vrati celu http odpoved servera
struct string http_connect(const char *path) { 
	
	printf("http connect path pred odseknutim prveho: %s a jeho dlzka je %d\n", path, strlen(path));

	struct string str;
	char *url = malloc(255*sizeof(char));
	strcpy(url,STR(SERVER_URL));
	
	printf("URL PO STRCPY pred strcat je %s\n", url);
	strcat(url,path);
	strcat(url,"/");
	
	printf("URL NA PRIPOJENIE JE %s\n", url);

	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl) {
		init_str(&str);	
		curl_easy_setopt(curl, CURLOPT_URL, url); 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writef); 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str); 
		res = curl_easy_perform(curl); 
		}	
	curl_easy_cleanup(curl);
	free(url);	
		
	return str;	
	
	}
	
//funkcia pre ziskanie obsahu konkretneho suboru
//volana z funkcie read	
struct string get_file_content(const char *path) { 
	
	struct string str;
	char *url = malloc(255*sizeof(char));
	strcpy(url,STR(SERVER_URL));	
	strcat(url,path);

	CURL *curl;
	CURLcode res;
	curl = curl_easy_init();
	if(curl) {
		init_str(&str);	
		curl_easy_setopt(curl, CURLOPT_URL, url); 
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writef); 
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &str); 
		res = curl_easy_perform(curl); 
		}	
	curl_easy_cleanup(curl);
	free(url);	
		
	return str;	
	
	}

//funkcia pre ziskanie obsahu adresara
struct entry *get_content(const char *path) {
	
	printf("get_content pre %s\n", path);
	
	struct string http_content = http_connect(path);
	struct entry *root = NULL;
	struct entry *curr = NULL;
	struct entry *tmp = NULL;
	char *i_d;
	char *tmp_d;
	char *p_d;
	char *r_d;
	char word[100];
	int i = 0;
	int i_ = 0;
	char *p;
	char c;
	
	while(sscanf(http_content.ptr+i, "%c", &c) != EOF) {
		i++;
		if(c=='<') {
			sscanf(http_content.ptr+i, "%[^>]", word);
			i = i + strlen(word);
				if(strstr(word,"a href")) {
					for(p = strtok(word, "\""); p!=NULL; p=strtok(NULL, "\"")) {
						if((i_%2 == 1) && (i_ > 9)) {
							tmp = (struct entry*)malloc(sizeof(struct entry));
	
							char *t = (char*)malloc((strlen(p))*sizeof(char));
							t = p;
							int k = strlen(t);
							if(t[strlen(t)-1] == '/') {								
								tmp_d = p;
								p_d = (char*)malloc(100);
								r_d = p_d;
								for(i_d = tmp_d; *i_d; i_d++ ) {
									if(*i_d != '/') {
									*p_d++ = *i_d;
									}
								}
								*p_d ='\0';
								strcpy(p,r_d);
								
								
							} 
							
							strcpy(tmp->name,p);
							tmp->next = NULL;
							
							if(root == NULL) {
								root = tmp;
								curr = tmp;
								}
			
							else {
								curr->next = tmp;
								curr = tmp;
							}	
								
							
													
							}
						i_++;
						}										
				}
			}
			
		}
		
		free(http_content.ptr);
		return root;	
	} 	
	
	
//funkcia, ktora urci, ci skumana polozka adresara je adresar, alebo subor	
int is_dir(const char *path) {
	static int pocet =0;
	
	counter++;
	
	printf("%d: do is_dir prisla premenna path : %s\n",counter,path);
	
	int is_dir = 0;
	int j, pos;
	for(j = strlen(path); j>= 0; j--){
		if(path[j] == '/') {
			pos = j;
			break;
			}
		}
		
	printf("%d: posledne lomitko je na pozicii %d\n",counter,pos);
	
	char *part1 = malloc(NAME_MAX*sizeof(char));
	char *part2 = malloc(NAME_MAX*sizeof(char));
	
	strncpy(part1, path, pos);
	part1[pos] = '\0';
	strncpy(part2, path+pos+1, strlen(path)-pos);
	part2[strlen(path)-pos] = '\0';
	
	
	printf("%d: part1 je %s part2 je %s\n",counter,part1,part2);
	
	struct string http_content = http_connect(part1);
		
	struct entry *root = NULL;
	struct entry *curr = NULL;
	struct entry *tmp = NULL;
	char *i_d;
	char *tmp_d;
	char *p_d;
	char *r_d;
	char word[100];
	int i = 0;
	int i_ = 0;
	char *p;
	char c;
	pocet++;
		
	
	while(sscanf(http_content.ptr+i, "%c", &c) != EOF) {
		//printf("%d: zacal while...\n", counter);
		i++;
		//printf("isdir while\n");
		if(c=='<') {
			sscanf(http_content.ptr+i, "%[^>]", word);
			i = i + strlen(word);
				if(strstr(word,"a href")) {
					for(p = strtok(word, "\""); p!=NULL; p=strtok(NULL, "\"")) {
						if((i_%2 == 1) && (i_ > 9)) {
							
							
							char *t = (char*)malloc((strlen(p))*sizeof(char));
							printf("alokuje sa velkost %d\n", strlen(p));
							strcpy(t,p);
							//int k = strlen(t);
							if(t[strlen(t)-1] == '/') {								
								tmp_d = p;
								p_d = (char*)malloc(100);
								r_d = p_d;
								for(i_d = tmp_d; *i_d; i_d++ ) {
									if(*i_d != '/') {
									*p_d++ = *i_d;
									}
								}
								*p_d ='\0';
								strcpy(p,r_d);
								printf("%d: po strcpy ... v porovnavacom stringu je %s a porovnava sa s %s\n",counter, p, part2);
								
								if(strcmp(p,part2) == 0) {
									is_dir = 1;
									break;
								}
								
							puts("ide sa uvolnit p_d");
							free(r_d);				
							puts("p_d uvolnene");								
							} 
							
							puts("ide sa uvolnit t");
							free(t);				
							puts("t uvolnene");			
							}
						i_++;
						}										
				}
			}
			
		}
		
		free(part1); 
		free(part2);
		free(http_content.ptr);
		
		printf("%d: is dir navratova hodnota je %d,pocet sprac.suborov %d\n",counter, is_dir,pocet);
		return is_dir;	
	} 
		
int get_size(const char *path) {
	
	int ret = 0;
	
	int j, pos;
	for(j = strlen(path); j>= 0; j--){
		if(path[j] == '/') {
			pos = j;
			break;
			}
		}
			
	char *part1 = malloc(NAME_MAX*sizeof(char));
	char *part2 = malloc(NAME_MAX*sizeof(char));
	
	strncpy(part1, path, pos);
	part1[pos] = '\0';
	strncpy(part2, path+pos+1, strlen(path)-pos);
	part2[strlen(path)-pos] = '\0';
		
	struct string http_content = http_connect(part1);
		
	int i = 0, count=0;
		char c;
		char word[100];
		int i_ = 0;
		char *p;
		
		struct SUBOR *head = NULL;
		struct SUBOR *current = NULL;
		struct SUBOR *temp = NULL;
		
		
		
		//get filenames
		
		while(sscanf(http_content.ptr+i, "%c", &c) != EOF) {
			i++;
			if(c=='<') {
				sscanf(http_content.ptr+i, "%[^>]", word);
				i = i + strlen(word);
				if(strstr(word,"a href")) {
					for(p = strtok(word, "\""); p!=NULL; p=strtok(NULL, "\"")) {
						if((i_%2 == 1) && (i_ > 9)) {
							
							temp = (struct SUBOR*)malloc(sizeof(struct SUBOR));
							strcpy(temp->fname,p);
							temp->next = NULL;
							
							if(head == NULL) {
								head = temp;
								current = temp;
								}
			
							else {
								current->next = temp;
								current = temp;
							}
							
							}
						i_++;
						}
					
					
				}
			}
			
		}
		
		//get file attributes
		
		i=0;
		regex_t regex;
		regex_t regex2;
		int comp_ret = regcomp(&regex, "([0-9]{4})-([0-9]{2})-([0-9]{2})", REG_EXTENDED);
		int comp_ret2 = regcomp(&regex2, "[[:space:]]", REG_EXTENDED);
		int reti;
		char *pch;
		char line[256];
		int counter = 0;
		
		temp = head;
		
		struct SUBOR *pok = head;
		int poc= 0;
		while(pok!=NULL) {
		pok=pok->next;
		poc++;
		}

		int datum = 0;
			
		while(sscanf(http_content.ptr+i, "%c", &c) != EOF) {
			i++;
			
			if(c=='>') {
				if (sscanf(http_content.ptr+i, "%[^<]", word)>0) {
				i = i + strlen(word);
				
				reti = regexec(&regex, word, 0, NULL, 0);
					
				if(!reti) {
					datum = 2;
				}
				if(!reti || (datum > 0)) {
					if(datum>0) datum--;
					memcpy(line, word, sizeof(word));
					for(pch = strtok(line, " "); pch!=NULL; pch=strtok(NULL, " ")) {
							reti = regexec(&regex2, pch, 0, NULL, 0);
							if(reti == REG_NOMATCH) {
								if(counter%3 == 0) {
									strcpy(temp->date, pch);
								}
								else if(counter%3 == 1) {
									strcpy(temp->time, pch);
									}
								else if(counter%3 == 2) {
									strcpy(temp->size, pch);
									temp = temp->next;
									}
								counter++;
								}
						}
					
				}
				}
			}
			
		}
		
		temp = head; 
		while(temp!=NULL) {
			if(strcmp(part2,temp->fname)==0) {
				if(strchr(temp->size,'K') != NULL) {
					char *x = malloc(sizeof(char));
					sscanf(temp->size,"%[^K]s",x);
					ret = atof(x)*1024;
					free(x);
					break;
				}
				else ret = atoi(temp->size);
				break;
				}
		
			temp = temp->next;
		}
		
		free_list2(head);
		
		free(part1); 
		free(part2);
		free(http_content.ptr);
		
		return ret;	
	} 

	
	
time_t get_time(const char *path) {
	
	time_t ret = time(NULL);
	
	int j, pos;
	for(j = strlen(path); j>= 0; j--){
		if(path[j] == '/') {
			pos = j;
			break;
			}
		}
			
	char *part1 = malloc(NAME_MAX*sizeof(char));
	char *part2 = malloc(NAME_MAX*sizeof(char));
	
	strncpy(part1, path, pos);
	part1[pos] = '\0';
	strncpy(part2, path+pos+1, strlen(path)-pos);
	part2[strlen(path)-pos] = '\0';
		
	struct string http_content = http_connect(part1);
		
	int i = 0, count=0;
		char c;
		char word[100];
		int i_ = 0;
		char *p;
		
		struct SUBOR *head = NULL;
		struct SUBOR *current = NULL;
		struct SUBOR *temp = NULL;
		
		char *i_d;
		char *tmp_d;
		char *p_d;
		char *r_d;
		
		//get filenames
		
		while(sscanf(http_content.ptr+i, "%c", &c) != EOF) {
			i++;
			if(c=='<') {
				sscanf(http_content.ptr+i, "%[^>]", word);
				i = i + strlen(word);
				if(strstr(word,"a href")) {
					for(p = strtok(word, "\""); p!=NULL; p=strtok(NULL, "\"")) {
						if((i_%2 == 1) && (i_ > 9)) {
							
							char *t = (char*)malloc((strlen(p))*sizeof(char));
							strcpy(t,p);
							if(t[strlen(t)-1] == '/') {								
								tmp_d = p;
								p_d = (char*)malloc(100);
								r_d = p_d;
								for(i_d = tmp_d; *i_d; i_d++ ) {
									if(*i_d != '/') {
									*p_d++ = *i_d;
									}
								}
								*p_d ='\0';
								strcpy(p,r_d);															
							} 
							
							temp = (struct SUBOR*)malloc(sizeof(struct SUBOR));
							strcpy(temp->fname,p);
							temp->next = NULL;
							
							if(head == NULL) {
								head = temp;
								current = temp;
								}
			
							else {
								current->next = temp;
								current = temp;
							}
							
							}
						i_++;
						}
					
					
				}
			}
			
		}
		
		//get file attributes
		
		i=0;
		regex_t regex;
		regex_t regex2;
		int comp_ret = regcomp(&regex, "([0-9]{4})-([0-9]{2})-([0-9]{2})", REG_EXTENDED);
		int comp_ret2 = regcomp(&regex2, "[[:space:]]", REG_EXTENDED);
		int reti;
		char *pch;
		char line[256];
		int counter = 0;
		
		temp = head;
		
		struct SUBOR *pok = head;
		int poc= 0;
		while(pok!=NULL) {
		pok=pok->next;
		poc++;
		}

		int datum = 0;
			
		while(sscanf(http_content.ptr+i, "%c", &c) != EOF) {
			i++;
			
			if(c=='>') {
				if (sscanf(http_content.ptr+i, "%[^<]", word)>0) {
				i = i + strlen(word);
				
				reti = regexec(&regex, word, 0, NULL, 0);
					
				if(!reti) {
					datum = 2;
				}
				if(!reti || (datum > 0)) {
					if(datum>0) datum--;
					memcpy(line, word, sizeof(word));
					for(pch = strtok(line, " "); pch!=NULL; pch=strtok(NULL, " ")) {
							reti = regexec(&regex2, pch, 0, NULL, 0);
							if(reti == REG_NOMATCH) {
								if(counter%3 == 0) {
									strcpy(temp->date, pch);
								}
								else if(counter%3 == 1) {
									strcpy(temp->time, pch);
									}
								else if(counter%3 == 2) {
									strcpy(temp->size, pch);
									temp = temp->next;
									}
								counter++;
								}
						}
					
				}
				}
			}
			
		}

		struct tm tm;
		int hh, mm, ss;
		int year, mday, mon;
		ss = 1;
		
		temp = head; 
		while(temp!=NULL) {
			
			if(strcmp(part2,temp->fname)==0) {
				sscanf(temp->time,"%i:%i", &hh, &mm);
				sscanf(temp->date,"%i-%i-%i", &year, &mon, &mday);
				tm.tm_year = year - 1900;
				tm.tm_mon = mon - 1;
				tm.tm_mday = mday;				
				tm.tm_hour = hh;
				tm.tm_min = mm;
				tm.tm_sec = ss;
				tm.tm_isdst = -1;
				ret = mktime(&tm);
				break;
				}
		
			temp = temp->next;
		}
		
		free_list2(head);	
		free(part1); 
		free(part2);
		free(http_content.ptr);
		
		return ret;	
	} 
	

static int fsgetattr(const char *path, struct stat *st) {

	//struct stat *pom = get_file_attributes(path);
	
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = get_time(path);
	st->st_mtime = get_time(path);
	
	
	if (strcmp(path,"/") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (is_dir(path) == 1) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	} 
	
	else {		
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		//st->st_size = 1024;	
		st->st_size = get_size(path);	
	}
	
	//free(pom);
	
	return 0;
	}

static int fsreaddir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *FI) {
				
	struct entry *subor_root = get_content(path);
	struct entry *subor_temp;
	
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	
	subor_temp = subor_root;
	while(subor_temp != NULL) {
		filler(buffer, subor_temp->name, NULL, 0);
		subor_temp = subor_temp->next;
		}
		
	free_list(subor_root);	
	return 0;
	
	}
	
static int fsread(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	

	char helloworld_cont[] = "Hello world !";
	char hello_cont[] = "Hello!";
	char *out = NULL; 
	
	if (strcmp(path,"/helloworld") == 0) {
		out = helloworld_cont;		
		}
	else if (strcmp(path,"/hello") == 0) {
		out = hello_cont;		
		}
	else {
		struct string filecontent = get_file_content(path);
		
		out = filecontent.ptr;
		
		}
	
	memcpy(buffer, out + offset, size);
	return strlen(out) - offset;
	
	}
	
static struct fuse_operations operations = {
	.getattr = fsgetattr,
	.readdir = fsreaddir,
	.read = fsread,	
	};
	
int main(int argc, char *argv[]) {

	return fuse_main(argc, argv, &operations, NULL);
}
