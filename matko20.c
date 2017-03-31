#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <wchar.h>
#include <Python.h>
#define FUSE_USE_VERSION 30
//#define _FILE_OFFSET_BITS 64
#include <fuse.h>

typedef struct file {
	char *name;
	int *size;
	char *date;
	char *time;
	} CFILE;
	
typedef struct dir {
	CFILE* this;
	struct dir *dirs;
	CFILE *files;
	} DIR;

typedef struct dfile {
	const char *name;
	long size;
	const char *date;
	const char *time;
	struct dfile *next_sibling;
	struct dfile *first_child;
	struct dfile *parent;
	char isdir;
	}DFILE;

#define TRUE 1
#define FALSE 0

void iterate_dict(PyObject*, DFILE*);
void alloc_properties(DFILE *, PyObject *);
const char* get_py_string(PyObject*);
void print_fs(DFILE *, int);

typedef int (*fuse_fill_dir_t) (void *buf, const char *name, const struct stat *sbuft, off_t off); 

//global variables
DFILE *root = (DFILE*)NULL;
char **dirs=NULL;
int dir_index=0;

static int fsgetattr(const char *path, struct stat *st) {
	
	printf("funkcia getattr() zavolana\n");
	printf("hladame atributy suboru %s\n",path);
	
	st->st_uid = getuid();
	st->st_gid = getgid();
	st->st_atime = time(NULL);
	st->st_mtime = time(NULL);
	
	/*
	if (strcmp(path,"/") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else if (strcmp(path,"/dir2") == 0) {
		st->st_mode = S_IFDIR | 0755;
		st->st_nlink = 2;
	}
	else {		
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;		
	}*/
	int i;
	char found=FALSE;
	for(i=0;i<dir_index;i++)
	{
		if(strcmp(path, dirs[i]) == 0)
		{
			st->st_mode = S_IFDIR | 0755;
			st->st_nlink = 2;
			found=TRUE;
		}
	}
	if(!found)
	{
		st->st_mode = S_IFREG | 0644;
		st->st_nlink = 1;
		st->st_size = 1024;
	}
	
	return 0;
	}
	
void fill_dir(const char *path, void *buffer, fuse_fill_dir_t filler) {
	DFILE *cur=NULL;
	//najdeme si spravny directory
	cur=root->first_child;
	char found = FALSE;
	char *path2  = strdup(path);
	if(strcmp(path,"/") == 0)
	{
		
		while(cur)
	{
		filler(buffer, cur->name, NULL, 0);
		cur=cur->next_sibling;
	}
	}
	else 
	{
		//musime najst spravny adresar na vypisanie
		const char s[2] = "/";
		char *token;
		/* get the first token */
		token = strtok(path2, s);
		/* walk through other tokens */
		
		while( token != NULL ) 
		{
			found=FALSE;
        //printf( " %s\n", token );
		while(cur!=NULL) 
			{
				if(strcmp(cur->name, token) != 0) 
				{
					cur=cur->next_sibling;
				}
				else
				{
					found=TRUE;
					cur=cur->first_child;
					break;
				}
			}
		if (found)
			token = strtok(NULL, s);
		else
		{
			break;
		}
		
		}
	if(found) //TODO: DOIMPLEMENTOVAT NEJAKE ROZUMNE RIESENIE NENAJDENIA SPRAVNEHO ADRESARA
	while(cur)
	{
		filler(buffer, cur->name, NULL, 0);
		cur=cur->next_sibling;
	}
	}
	
	
	}

static int fsreaddir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *FI) {
	
	printf("vypisujem subory v adresari %s/n", path);
	
	filler(buffer, ".", NULL, 0);
	filler(buffer, "..", NULL, 0);
	
	if(strcmp(path,"/") == 0) {
		filler(buffer, "helloworld", NULL, 0);
		filler(buffer, "hello", NULL, 0);
		fill_dir(path,buffer,filler);		
		}
	else {
		fill_dir(path,buffer,filler);
	}
		
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
	else return -1;
	
	memcpy(buffer, out + offset, size);
	return strlen(out) - offset;
	
	}
	
static struct fuse_operations operations = {
	
	.getattr = fsgetattr,
	.readdir = fsreaddir,
	.read = fsread,
	
	};

int main(int argc, char *argv[]) {
	//int argc;
	//wchar_t * argv[3];
	setlocale(LC_ALL, "en_US.UTF-8");
	//argc=2;
	//argv[0]=L"mypy.py";
	//argv[1]=L"http://broskini.xf.cz/folder1";
	char *filename = "mypy20"; //meno pythonackeho modulu na import
	PyObject *sName, *sModule, *sFunc, *sValue, *sysPath;
	PyObject *sLong;
	long int d=4;
	root=(DFILE*)malloc(sizeof(DFILE));
	
	Py_Initialize();
	
	sLong=PyLong_FromLong(d);
	sysPath = PySys_GetObject("path");
	long len = PySequence_Length(sysPath);
	printf("Povodne je v path je %ld poloziek.\n", len);
	
	PyObject *sitem;
	int i;
	for (i=0;i<len;i++) {
	sitem = PySequence_GetItem(sysPath,(Py_ssize_t) i);
	PyObject *ascii = PyUnicode_AsASCIIString(sitem);
	char *compname = PyBytes_AsString(ascii);
	Py_DECREF(ascii);
	printf("Path %d.: %s\n", i+1,compname);
	}
	
	fflush(stdout);
	
	//PySys_SetArgv(argc,argv);
	//file=fopen("mypy.py","r");
	//PyRun_SimpleFile(file, "mypy.py");
	
	PyRun_SimpleString(
		"import sys\n"
		"sys.path.append('/home/pi/Documents/C/matko')\n"
	);
	
	sysPath = PySys_GetObject("path");
	len = PySequence_Length(sysPath);
	printf("Po pridani je v path je  %ld poloziek.\n", len);
	
	for (i=0;i<len;i++) {
	sitem = PySequence_GetItem(sysPath,(Py_ssize_t) i);
	PyObject *ascii = PyUnicode_AsASCIIString(sitem);
	char *compname = PyBytes_AsString(ascii);
	Py_DECREF(ascii);
	printf("Path %d.: %s\n", i+1,compname);
	}
	
	int kind = PyUnicode_1BYTE_KIND;
	sName=PyUnicode_FromKindAndData(kind, filename, (Py_ssize_t) 6);
	sModule = PyImport_Import(sName);
	Py_DECREF(sName);
	
	if (sModule != NULL)
	{
		sFunc = PyObject_GetAttrString(sModule, "main");
		if (sFunc && PyCallable_Check(sFunc))
		{
			sValue = PyObject_CallObject(sFunc, NULL);
			
		}
		else
		{
			printf("SFUNC AND CHYBA!!\n");
		}
		if (sValue != NULL)
		{
			printf("PYTHON FUNKCIA VRATILA OBJEKT!!!!!!\n");
		}
		else
		{
			printf("NIC MI SEM NEPRISLO!!!!\n");
		}
		
	}
	else 
	{
		printf("SMODULE JE NULL!!!\n");
	}
	
	long dict_check = PyDict_CheckExact(sValue);
	if( dict_check == 0)
	{
		printf("Vrateny python objekt nie je dict, koncim program..\n");
		return 1;
	}
	printf("Velkost dictu je %d\n", PyDict_Size(sValue));
	
	
	root->isdir=1;
	root->name = "root";
	root->size=0;
	root->parent = NULL;
	root->next_sibling = NULL;
	root->first_child=NULL;
	
	PyObject *files = PyDict_GetItemString(sValue, "files");
	int dirs_count=0;
	iterate_dict(files,root,&dirs_count);
	printf("---- ZACINAM VYPISOVANIE V C ----\n");
	//allocate dirs char array
	dirs=(char**)malloc(dirs_count * sizeof(char*));
	//for(i=0;i<dirs_count;i++) dirs[i]=
	char *curdir=malloc(2*sizeof(char));
	strcat(curdir,"/");
	print_fs(root,0,curdir);
	Py_DECREF(sLong);
	Py_DECREF(files);
	Py_DECREF(sValue);
	Py_DECREF(sModule);
	
	
	Py_Finalize();
	
		
	return fuse_main(argc, argv, &operations, NULL);
	
	}
	
	//TODO: aj tuto treba vyriesit tie INCREF/DECREF lebo to bude potom
	//zbytocne zaberat pamat -- keby sa program dlho pouziva mohla by sa
	//aj vycerpat teoreticky
void iterate_dict(PyObject *sValue, DFILE *root,int *dirs_count) {
	PyObject *key, *value;
	Py_ssize_t pos = 0;
	
	DFILE* cur;
	while(PyDict_Next(sValue, &pos, &key, &value))
	{
		if(root->first_child == NULL)
		{
			cur = (DFILE*)malloc(sizeof(DFILE));
			root->first_child = cur;
			cur->parent=root;
			cur->next_sibling=NULL;
			cur->first_child=NULL;
		}
		else
		{
			cur->next_sibling = (DFILE*)malloc(sizeof(DFILE));
			cur = cur->next_sibling;
			cur->next_sibling=NULL;
			cur->parent = root;
			cur->first_child=NULL;
		}

		cur->name = get_py_string(key);
		//printf("Key je: %s\n", cur->name);
		alloc_properties(cur, value);
		if(cur->isdir == 1)
		{
			++(*dirs_count);
			PyObject *files = PyDict_GetItemString(value, "files");
			//Py_INCREF(files);
			iterate_dict(files,cur);
			//Py_DECREF(files);
		}
	}
}

void alloc_properties(DFILE* cur, PyObject *dict) 
{
	PyObject *date = PyDict_GetItemString(dict, "date");
	PyObject *time = PyDict_GetItemString(dict, "time");
	PyObject *size = PyDict_GetItemString(dict, "size");
	PyObject *isdir = PyDict_GetItemString(dict, "isdir");
	cur->date=get_py_string(date);
	cur->time=get_py_string(time);
	const char *size_str = get_py_string(size);
	if (strcmp(size_str,"-") == 0)
	{
		cur->size=0;
	}
	cur->size=strtol(size_str,NULL,10);
	//PyLong_AsLong(size);
	if(PyLong_AsLong(isdir) == 1)
	{
		cur->isdir=1;
	}
	else
	{
		cur->isdir=0;
	}
	
	
}

//TODO: tuto v tejto funkcii je mozne ze sa globalne alokuju veci ktore sa
//potom neuvolnuju (lebo som to tak nenapisal)
//treba to zistit a vyriesit
const char* get_py_string(PyObject *string)
{
	PyObject *ascii = PyUnicode_AsASCIIString(string);
	const char *compname = PyBytes_AsString(ascii);	
	Py_DECREF(ascii);
	return strdup(compname);
}
	
void print_fs(DFILE *root, int depth, char *current_dir)
{
	int i;
	DFILE *cur;
	cur=root->first_child;
	char *next_dir=NULL;
	dirs[dir_index]=current_dir;
	dir_index++;
	while(cur!=NULL)
	{
		for(i=0;i<depth;i++) printf("\t");
		if(cur->isdir)
		{
			printf("%s/\n",cur->name);
			int alloc_size = strlen(current_dir) + strlen(cur->name) + 2;
			next_dir=(char*)malloc(alloc_size*sizeof(char));
			strcat(next_dir,current_dir);
			strcat(next_dir,cur->name);
			strcat(next_dir,"/");
			print_fs(cur,depth+1,next_dir);	
		}
		else
		{
			printf("%s\n",cur->name);
		}
		cur=cur->next_sibling;
	}
	
	
}
