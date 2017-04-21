import urllib3
import time
import pprint
import re
import codecs
import requests
import sys
 
from bs4 import BeautifulSoup


http_status_errors = 0


def get_dir_files(input_url):
    page=requests.get(input_url) 
    if page.status_code != 200: #nieco neprebehlo ok, vraciame prazdny dict
        http_status_errors+=1
        return {}
    soup = BeautifulSoup(page.content, "lxml") #vyparsuje content cez lxml parser - on ma vlastny strom, tagy su nody, blablabla
    pre = soup.find("pre") #v pre tagu su linky
    s = pre.get_text() #textova podoba obsahu pre tagu
    s = re.sub('  +', ' ', s) #squeeznu sa viacere medzery na jednu
    s = s[s.find('\n')+1:] #nejake odriadkovanie?..prvy riadok boli hovadiny
    cnt = 0 #cnt je pozicia slova --- najskor meno,datum,cas, velkost
    root = {} #lokalny root co sa da do main rootu
    name = ""
    for word in s.split(): #text s sa rozdeli podla medier
        cnt+=1 
        if cnt % 4 == 1: #prva poz je meno
            name=word
            name=name.replace("/","") #da sa prec lomitko
            root[name]={} #dalsi dict pre aktualny subvor/adresar
            if '/' in word:
                root[name]["isdir"]=1 # is a directory
                root[name]['files']={}
            else:
                root[name]["isdir"]=0 # is not a directory
        elif cnt % 4 == 2:
            root[name]["date"]=word
        elif cnt % 4 == 3:
            root[name]["time"]=word
        elif cnt % 4 == 0:
            if word.find('K') != -1:
                #print("obsahuje Kcko")
                word = re.sub('K','',word)
                word = float(word)
                word = word*1024
                word = repr(word)
                root[name]["size"]=word
            else:
                root[name]["size"]=word
    
    #for key,value in root.items():  #pre vsetky subory v aktualnom priecinku
       #print ("Nazov:" + key + " datum:" + root[key]["date"] + " cas: " + root[key]["time"] + " velkost:" + root[key]["size"] + " directory: " + str(root[key]["isdir"]))
    return root
 
def dirs_recursive_iterate(root, relative_path):
    for key,val in root['files'].items():
        if val['isdir'] == 1:
            nextpath=''
            nextpath+=relative_path+'/'+key #key je vlastne meno suboru
            val['files']=get_dir_files(nextpath)
            dirs_recursive_iterate(val,nextpath)
        else:
            continue #ked to nie je dictionary nic s tym dalej robit nemusime

def print_fs_structure(root, depth):
    for key,val in root['files'].items():
        if val['isdir'] == 1 and 'files' in val and len(val['files']) > 0:
            print ('\t'*depth+key+'/')
            print_fs_structure(val,depth+1)
        else:
            print ('\t'*depth+key)
 
def main(url, mode):

    #if len(sys.argv) > 1:
    #    ca_one  = str(sys.argv[1])
   #     print ("PRVY ARGUMENT JE " + str(ca_one))
    
 
    #for link in soup.findAll('a'):
    #   fullurl=link.get("href")
    #   print (fullurl)

    root={}  #dictionary - kazdy priecinok je dictionary, aj subor ...vsetko adresare aj subory su dictionary
    root['files']=get_dir_files(url)
    if mode == "T":
	    dirs_recursive_iterate(root,url)
    print_fs_structure(root,0) #vypiseme si celkovu suborovu strukturu nasho fs
    
    return root
 
 
if __name__  == "__main__":
    base_url = "http://broskini.xf.cz/folder1"
    mode = "R"
    main(base_url, mode)
    print ("HTTP status errors: " + str(http_status_errors))
