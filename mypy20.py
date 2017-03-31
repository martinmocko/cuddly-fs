import urllib3
import time
import pprint
import re
import codecs
import requests
import sys
 
from bs4 import BeautifulSoup

base_url = "http://broskini.xf.cz/folder1"
http_status_errors = 0

def get_dir_files(input_url):
    page=requests.get(input_url)
    if page.status_code != 200: #nieco neprebehlo ok, vraciame prazdny dict
        http_status_errors+=1
        return {}
    soup = BeautifulSoup(page.content, "lxml")
    pre = soup.find("pre")
    s = pre.get_text()
    s = re.sub('  +', ' ', s)
    s = s[s.find('\n')+1:]
    cnt = 0
    root = {}
    name = ""
    for word in s.split():
        cnt+=1
        if cnt % 4 == 1:
            name=word
            name=name.replace("/","")
            root[name]={}
            if '/' in word:
                root[name]["isdir"]=1 # is a directory
            else:
                root[name]["isdir"]=0 # is not a directory
        elif cnt % 4 == 2:
            root[name]["date"]=word
        elif cnt % 4 == 3:
            root[name]["time"]=word
        elif cnt % 4 == 0:
            root[name]["size"]=word
    
    #for key,value in root.items():
    #   print ("Nazov:" + key + " datum:" + root[key]["date"] + " cas: " + root[key]["time"] + " velkost:" + root[key]["size"] + " directory: " + str(root[key]["type"]))
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
        if val['isdir'] == 1:
            print ('\t'*depth+key+'/')
            print_fs_structure(val,depth+1)
        else:
            print ('\t'*depth+key)
 
def main():

    #if len(sys.argv) > 1:
    #    ca_one  = str(sys.argv[1])
   #     print ("PRVY ARGUMENT JE " + str(ca_one))
    
 
    #for link in soup.findAll('a'):
    #   fullurl=link.get("href")
    #   print (fullurl)

    root={}
    root['files']=get_dir_files(base_url)
    dirs_recursive_iterate(root,base_url)
    #print_fs_structure(root,0) #vypiseme si celkovu suborovu strukturu nasho fs
    
    return root
 
 
if __name__  == "__main__":
    main()
    print ("HTTP status errors: " + str(http_status_errors))
