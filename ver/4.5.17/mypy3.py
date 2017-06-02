import urllib3
import time
import pprint
import re
import codecs
import requests
import sys
 
from bs4 import BeautifulSoup
import dateutil.parser as dparser

http_status_errors = 0

def get_file_size(size):
    word=size
    if size.find('K') != -1:
        word = re.sub('K','',word)
        word = float(word)
        word = word*1024
        word += 1024
        word = repr(word)
    elif size.find('M') != -1:
        word = re.sub('M','',word)
        word = float(word)
        word = word*1024*1024
        word += 1024*1024
        word = repr(word)
    elif size.find('G') != -1:
        word = re.sub('G','',word)
        word = float(word)
        word = word*1024*1024*1024
        word += 1024*1024*1024
        word = repr(word)
    return word
            

def current_folder_print(node):
    for key,value in node.items():  #pre vsetky subory v aktualnom priecinku
       print ("Nazov:" + key + " datum:" + node[key]["date"] + " cas:" + node[key]["time"] + " velkost:" + node[key]["size"] + " directory: " + str(node[key]["isdir"]))

def apache(table,input_url):
    tr_tags = table.findAll("tr")
    counter=0
    root={}
    if input_url.endswith('/') == False:
        input_url += '/'
        
    for tag in tr_tags:
        counter+=1
        if(counter == 1 or counter ==2 or counter == 3):
            continue #preskakujeme zopar prvych tr tagov lebo to nie tagy kde su subory
        if counter == len(tr_tags):
            continue #posledny tr tag v tabulke tiez neni o subore
        #print (tag)
        td_counter=0
        for td in tag:
            td_counter+=1
            if td_counter == 1:
                continue #preskakujeme prvy td tag lebo je tam len obrazok
            elif td_counter == 2:
                #nazov suboru
                word = td.get_text()
                name = word
                name=name.replace("/","") #da sa prec lomitko
                root[name]={} #dalsi dict pre aktualny subor/adresar
                if '/' in word:
                    root[name]["isdir"]=1 # is a directory
                    root[name]['files']={}
                else:
                    root[name]["isdir"]=0 # is not a directory
            
            elif td_counter == 3:
                #datum a cas
                text=td.get_text()
                date,time = text.split()[0:2]
                date = date.strip() #odstranit leading a trailing whitespaces, len pre istotu
                time = time.strip() #to iste
                parsed_date=dparser.parse(date,fuzzy=True)
                date = parsed_date.strftime("%Y-%m-%d")
                root[name]["date"]=date
                root[name]["time"]=time
                
            elif td_counter == 4:
                #velkost
                if root[name]["isdir"] == 1:
                    root[name]["size"]="-"
                    continue
                #file_url = input_url + name
                #print ("REQUESTUJEM TUTO URL " + file_url)
                #rq = requests.get(file_url,stream=True)
                #root[name]["size"]=rq.headers['Content-length']
                #rq.close()
                size = td.get_text()
                size = size.strip()
                root[name]["size"]=get_file_size(size)
                
    #current_folder_print(root)
                
    return root

def nginx(pre,input_url):
    s = pre.get_text() #textova podoba obsahu pre tagu
    s = re.sub('  +', ' ', s) #squeeznu sa viacere medzery na jednu
    s = s[s.find('\n')+1:] #nejake odriadkovanie?..prvy riadok boli hovadiny
    cnt = 0 #cnt je pozicia slova --- najskor meno,datum,cas, velkost
    root = {} #lokalny root co sa da do main rootu
    name = ""
    for word in s.split(): #text s sa rozdeli podla medzier
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
            if root[name]["isdir"] == 1:
                root[name]["size"]="-"
                continue
            #file_url = input_url + name
            #print ("REQUESTUJEM TUTO URL " + file_url)
            #rq = requests.get(file_url,stream=True)
            #root[name]["size"]=rq.headers['Content-length']
            #rq.close()
            size = word.strip()
            root[name]["size"]=get_file_size(size)
    
    return root

def get_dir_files(input_url):
    page=requests.get(input_url) 
    if page.status_code != 200: #nieco neprebehlo ok, vraciame prazdny dict
        http_status_errors+=1
        return {}
    soup = BeautifulSoup(page.content, "lxml") #vyparsuje content cez lxml parser - on ma vlastny strom, tagy su nody, blablabla
    
    pre = soup.find("pre") #v pre tagu su linky
    if pre != None:
        print ("********************** PRE TAG **********************")
        return nginx(pre,input_url)
    table = soup.find("table")
    if table != None:
        print ("********************** TABLE TAG **********************")
        return apache(table,input_url)
        
    
 
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
    #print_fs_structure(root,0) #vypiseme si celkovu suborovu strukturu nasho fs
    
    return root
 
 
if __name__  == "__main__":
    base_url = "http://broskini.xf.cz/folder1"
