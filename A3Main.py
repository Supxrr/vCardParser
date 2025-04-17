#!/usr/bin/env python3
import os 
import sys
import time
import datetime
import mysql.connector
from asciimatics.widgets import Frame, ListBox, Layout, Label, Divider, Text, \
    Button, TextBox, Widget
from asciimatics.scene import Scene
from asciimatics.screen import Screen
from asciimatics.exceptions import ResizeScreenError, NextScene, StopApplication
import mysql.connector
import sys
import sqlite3
import ctypes 
from ctypes import CDLL
from ctypes import c_char_p
from ctypes import c_int



#-------------------C WRAPPER FUNCTIONS-------------------
#my shared library
lib_path = os.path.join(os.path.dirname(__file__), "../bin/libvcparser.so")
lib = CDLL(lib_path)


#set the arguement and return types of the wrapper function
lib.getCardSummary.argtypes = [c_char_p]
lib.getCardSummary.restype = c_char_p

lib.updateCard.argtypes = [c_char_p, c_char_p]
lib.updateCard.restype = c_int


def get_vcard_summary(filename):


    #convert the python string to a c string
    summary_ptr = lib.getCardSummary(filename.encode('utf-8'))
    if not summary_ptr:
        return "Error: Could not get summary"
    
    #convert the returned c string to a python string
    summary = summary_ptr.decode('utf-8')

    return summary

def update_vcard(filename, new_fn):
    return lib.updateCard(filename.encode('utf-8'), new_fn.encode('utf-8'))

def create_new_card(filename, fn_value):
    return lib.createNewCard(filename.encode('utf-8'), fn_value.encode('utf-8'))

#-------------------DATABASE FUNCTIONS-------------------
#global variable to store the connection
db_connection = None

#helper function to connect to the database
def connect_to_db(username, password, dbname):
    conn = mysql.connector.connect(
        host="dursley.socs.uoguelph.ca",
        user=username,
        password=password,
        database=dbname
    )
    conn.autocommit = True
    return conn


#CREATE THE TABLE FUNCTIONS
def create_tables(conn):
    cursor = conn.cursor()

    #CREATE FILE TABLE

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS FILE (
            file_id INT AUTO_INCREMENT PRIMARY KEY,
            file_name VARCHAR(60) NOT NULL,
            last_modified DATETIME,
            creation_time DATETIME NOT NULL
        );
    """)

    #CREATE CONTACT TABLE WITH FOREIGN KEY CONSTRAINT
    
    cursor.execute("""
        CREATE TABLE IF NOT EXISTS CONTACT (
            contact_id INT AUTO_INCREMENT PRIMARY KEY,
            name VARCHAR(256) NOT NULL,
            birthday DATETIME,
            anniversary DATETIME,
            file_id INT NOT NULL,
            FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE 
        );
   """)
    conn.commit()
    cursor.close()

def insert_file_record(conn, file_name, full_path):

    """
    If the file is not already in the file table, insert it
    return file id
    """

    #check if the file is already in the table
    cursor = conn.cursor()
    cursor.execute("SELECT file_id FROM FILE WHERE file_name = %s", (file_name,))
    row = cursor.fetchone()

    if row:
        file_id = row[0]
    else:
        last_modified = os.path.getmtime(full_path)
        dt_str = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(last_modified))
        cursor.execute("INSERT INTO FILE (file_name, last_modified, creation_time) VALUES (%s, %s, NOW())", (file_name, dt_str))
        file_id = cursor.lastrowid
    conn.commit()
    cursor.close()

    return file_id

def insert_contact_record(conn, file_id, name, birthday, anniversary):

    """
    Insert a contact record using the given information
    empty strings are converted to NULL
    """

    cursor = conn.cursor()
    birthday_val = birthday if birthday and birthday.upper() != "NULL" else None
    anniversary_val = anniversary if anniversary and anniversary.upper() != "NULL" else None
    cursor.execute("INSERT INTO CONTACT (name, birthday, anniversary, file_id) VALUES (%s, %s, %s, %s)", (name, birthday_val, anniversary_val, file_id))
    conn.commit()
    cursor.close()

def run_display_all_contacts(conn):

    """
    Run the query to display all contacts in the database
    """

    cursor = conn.cursor()

    query = """
        SELECT c.name, c.birthday, c.anniversary, f.file_name
        FROM CONTACT c JOIN FILE f ON c.file_id = f.file_id
        ORDER BY c.name;
    """
    cursor.execute(query)
    rows = cursor.fetchall()
    cursor.close()

    result = "Contacts:\n"
    for row in rows:
        result += f"Name: {row[0]}, Birthday: {row[1]}, Anniversary: {row[2]}, File: {row[3]}\n"
    return result




#-------------------HELPER-------------------
def parse_vcard_summary_for_main_fields(summary):
    
    lines = summary.splitlines()
    fn = ""
    birthday = ""
    anniversary = ""

    found_fn_block = False
    found_bday_block = False
    found_anniv_block = False

    for line in lines:
        line = line.strip()
        if line.startswith("Full Name:"):
            found_fn_block = True
            continue
        
        if found_fn_block:
            #watch for lines starting with "- "
            if line.startswith("- "):
                fn = line[1:].strip()
                found_fn_block = False
            if line == "" or line.startswith("---Optional"):
                found_fn_block = False
        
        if line.startswith("Birthday:"):
            found_bday_block = True
            continue
        
        if found_bday_block:
            if line.strip() and not line.startswith("---"):
                birthday = line.strip()
            found_bday_block = False
        
        if line.startswith("Anniversary:"):
            found_anniv_block = True
            continue
        
        if found_anniv_block:
            if line.strip() and not line.startswith("---"):
                anniversary = line.strip()
            found_anniv_block = False

    #convert null placeholders to empty strings
    if birthday == "NULL":
        birthday = ""
    
    if anniversary == "NULL":
        anniversary = ""
    
    return fn, birthday, anniversary




#helper to parse date time to make the output look like the assignment instructions.
def format_datetime_for_display(raw_dt):
    #maybe partial or text based
    if not raw_dt or 'T' not in raw_dt:
        return raw_dt
    
    #attempt to parse
    has_utc = raw_dt.endswith('Z')
    trimmed = raw_dt.rstrip('Z')

    #split on T
    parts = trimmed.split('T')

    if len(parts) != 2:
        return raw_dt
    
    date_part, time_part = parts

    if has_utc:
        return f"Date: {date_part} Time: {time_part} (UTC)"
    else:
        return f"Date: {date_part} Time: {time_part}"

#helper to populate the DB from the cards
def populate_db_from_cards(conn, folder):
    all_files = [f for f in os.listdir(folder) if f.endswith((".vcf", ".vcard"))]
    for f in all_files:
        full_path = os.path.join(folder, f)
        #get summary
        summary = get_vcard_summary(full_path)
        #parse out the fields
        fn, bday, anniv = parse_vcard_summary_for_main_fields(summary)
        if not fn:
            print(f"Error: Could not parse name from {f}")
            continue
        #insert into FILE
        file_id = insert_file_record(conn, f, full_path)


        #check if a contact record already exists for this file id
        cursor = conn.cursor()
        cursor.execute("SELECT contact_id FROM CONTACT WHERE file_id = %s", (file_id,))
        contact_row = cursor.fetchone()
        cursor.close()

        if not contact_row:
            #then into CONTACT
            insert_contact_record(conn, file_id, fn, bday, anniv)


#step 1: Model: VCardModel
#-------------------UI MODEL-------------------
class VCardModel():
    def __init__(self, folder="cards"):
        self.folder = folder
        self.valid_files = self.scan_cards()
        self.current_filename = None #store current file name
    
    def scan_cards(self):
        valid_files = []
        if not os.path.isdir(self.folder):
            return valid_files
        
        for file in os.listdir(self.folder):
            if file.endswith((".vcf", ".vcard")):
                full_path = os.path.join(self.folder, file)
                summary = get_vcard_summary(full_path)

                #only add the file if the summary does not contain an error
                if not summary.startswith("Error:"):
                    valid_files.append(file)
                else:
                    print(f"Error: Could not read {file}")
        return valid_files

    def get_summary(self):
        #return list of tuples
        return [(f, f) for f in self.valid_files]


#step 2 View; ListView & detailsView
class ListView(Frame):
    def __init__(self, screen, model):
        super(ListView, self).__init__(screen,
                                       screen.height * 2 // 3,
                                       screen.width * 2 // 3,
                                       on_load=self._on_load_main,
                                       hover_focus=True,
                                       can_scroll=True,
                                       title="vCard List")
        self._model = model

        #listbox shows file names
        self._list_view = ListBox(Widget.FILL_FRAME,
                                  model.get_summary(),
                                  name="vcards",
                                  add_scroll_bar=True,
                                  on_change=self._on_pick,
                                  on_select=self._edit)
        
        #buttons at the bottom
        self._create_button = Button("Create", self._create)
        self._edit_button = Button("Edit", self._edit)
        self._db_button = Button("DB queries", self._db_queries)
        self._exit_button = Button("Exit", self._quit)

        #layout
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(self._list_view)
        layout.add_widget(Divider())

        layout2 = Layout([1, 1, 1, 1])
        self.add_layout(layout2)
        layout2.add_widget(self._create_button, 0)
        layout2.add_widget(self._edit_button, 1)
        layout2.add_widget(self._db_button, 2)
        layout2.add_widget(self._exit_button, 3)
        self.fix()
        self._on_pick()

    
    def _on_pick(self):
        self._edit_button.disabled = self._list_view.value is None
    
    
    def _on_load_main(self):
        #called once we switch to the main scene
        global db_connection
        if db_connection is not None:
            populate_db_from_cards(db_connection, self._model.folder)
        else:
            print("Error: DB connection not established")
        #then refresh
        self._model.valid_files = self._model.scan_cards()
        self._list_view.options = self._model.get_summary()
        self._list_view.value = None
        self._on_pick()
    
    def _create(self):
        
        self.save()
        #Will call C wrapper function to create a new vcard object
        self._model.current_filename = None #create a new vcard
        
        raise NextScene("Details")
    
    def _edit(self):
        #switch to details view for editing a vcard
        self.save()
        selected_file = self._list_view.value
        #Will call C wrapper function to load the vcard object
        self._model.current_filename = selected_file
        raise NextScene("Details")
    
    def _db_queries(self):
        raise NextScene("DBQueries")
    
    @staticmethod
    def _quit():
        print("[DEBUG] Exit button pressed")
        raise StopApplication("User pressed Exit")


#step 3: Details view
class DetailsView(Frame):
    def __init__(self, screen, model):
        super(DetailsView, self).__init__(screen,
                                          screen.height * 2 // 3,
                                          screen.width * 2 // 3,
                                          title="vCard Details",
                                          hover_focus=True,
                                          can_scroll=False)
        #layout for fields
        self._model = model
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)
        layout.add_widget(Text("File Name:", "filename"))
        layout.add_widget(Text("Contact:", "contact"))
        layout.add_widget(Text("Birthday:", "birthday"))
        layout.add_widget(Text("Anniversary", "anniversary"))
        layout.add_widget(Text("Other Properties:", "other"))

        #buttons
        layout2 = Layout([1, 1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        self.fix()
    
    def reset(self):
        super(DetailsView, self).reset()
        
        
        #if editing an existing vcard, load details from the file using our wrapper function in C
        #create mode so dont allow for editing of the other fields
        if self._model.current_filename is None:

            self.data = {
                "filename": "",
                "contact": "",
                "birthday": "",
                "anniversary": "",
                "other": "0"
            }

            #only allow editing of the contact field and filename in create mode
            self.find_widget("filename").disabled = False
            self.find_widget("contact").disabled = False



            self.find_widget("birthday").disabled = True
            self.find_widget("anniversary").disabled = True
            self.find_widget("other").disabled = True

        else:
            #editing an existing vcard
            full_path = os.path.join(self._model.folder, self._model.current_filename)
            summary = get_vcard_summary(full_path)

            #parse the summary string
            fn = ""
            birthday = ""
            anniversary = ""
            other_count = "0"

            #parser the lines of cardtostring
            lines = summary.splitlines()

            #parse the lines
            found_fn_block = False
            found_bday_block = False
            found_anniv_block = False
            found_other_block = False
            other_count_int = 0


            for i, line in enumerate(lines):
                line = line.strip()

                #attempt to parse the block after FN:
                if line.startswith("Full Name:"):
                    #next lines might contain name or values
                    found_fn_block = True
                    continue
                
                if found_fn_block:
                    #parse the name
                    if line.startswith("Name: ") or line.startswith("Parameters:"):
                        continue
                    elif line.startswith("Values:"):
                        #next line might be the name
                        continue
                    elif line.startswith("- "):
                        fn = line[1:].strip()
                        found_fn_block = False
                    if line == "" or line.startswith("---Optional Properties---"):
                        found_fn_block = False
                
                #attempt to parse the block after BDAY:
                if line.startswith("Birthday:"):
                    #next lines might contain name or values
                    found_bday_block = True
                    continue
                
                if found_bday_block:
                    if line.strip() and not line.startswith("---"):
                        birthday = line.strip()
                    found_bday_block = False
                
                #attempt to parse the block after ANNIVERSARY:
                if line.startswith("Anniversary:"):
                    #next lines might contain name or values
                    found_anniv_block = True
                    continue
                
                if found_anniv_block:
                    if line.strip() and not line.startswith("---"):
                        anniversary = line.strip()
                    found_anniv_block = False
                
                #attempt to parse the block after OTHER:
                if line.startswith("---Optional Properties---"):
                    #next lines might contain name or values
                    found_other_block = True
                    continue
                
                if found_other_block:
                    #keep going until see bday or anniv or end
                    if line.startswith("Birthday:") or line.startswith("Anniversary:") or line.startswith("---End of Card---"):
                        found_other_block = False
                    else:
                        if line.startswith("Name: "):
                            other_count_int += 1
                
            other_count = str(other_count_int)

            #parse into format
            birthday = format_datetime_for_display(birthday)
            anniversary = format_datetime_for_display(anniversary)

            #if bday and anniv were null
            if birthday == "NULL":
                birthday = ""
            if anniversary == "NULL":  
                anniversary = ""
                        
                    
            #set the fields
            self.data = {
                "filename": self._model.current_filename,
                "contact": fn,
                "birthday": birthday,
                "anniversary": anniversary,
                "other": other_count
            }

            #only allow editing of the contact field in edit mode
            self.find_widget("contact").disabled = False
            self.find_widget("filename").disabled = True
            self.find_widget("birthday").disabled = True
            self.find_widget("anniversary").disabled = True
            self.find_widget("other").disabled = True

    def _ok(self):
        global db_connection
        #save the vcard object
        self.save()
        #will call the C functions to update the card
        filename = self.data["filename"].strip()
        contact = self.data["contact"].strip()

        #if user did not provide a filename
        if not filename:
            print("Error: No filename provided")
            return
        
        if not contact:
            print("Error: No contact name provided")
            return
        
        #if we are creating a new card ensure vcf
        if not filename.lower().endswith((".vcf", ".vcard")):
            filename += ".vcf"

        full_path = os.path.join(self._model.folder, filename)

        #means create card
        if self._model.current_filename is None:

            result = create_new_card(full_path, contact)

            if result == 0:
                print("New card created")
                #insert into DB
                global db_connection
                if db_connection is not None:
                    file_id = insert_file_record(db_connection, filename, full_path)
                    insert_contact_record(db_connection, file_id, contact, self.data["birthday"], self.data["anniversary"])
                else:
                    print("Error: DB connection not established")
                
            else:
                print("Error: Could not create new card")
                return
        else:
            
            #edit mode
            result = update_vcard(full_path, contact)
            if result == 0:
                print("Card updated")
                #update the DB
                if db_connection is not None:
                    cursor = db_connection.cursor()
                    cursor.execute("SELECT file_id FROM FILE WHERE file_name = %s", (self._model.current_filename,))
                    row = cursor.fetchone()
                    if row:
                        file_id = row[0]
                        cursor.execute("UPDATE CONTACT SET name = %s, birthday = %s, anniversary = %s WHERE file_id = %s", (contact, self.data["birthday"], self.data["anniversary"], file_id))
                        db_connection.commit()
                    cursor.close()
                else:
                    print("Error: DB connection not established")
            else:
                print("Error: Could not update card")
                return
        raise NextScene("Main")

    def _cancel(self):
        raise NextScene("Main")

class DBQueryView(Frame):
    def __init__(self, screen):
        super(DBQueryView, self).__init__(screen,
                                          screen.height * 2 // 3,
                                          screen.width * 2 // 3,
                                          title="DB Queries",
                                          hover_focus=True,
                                          can_scroll=False)
        #layout for fields
        layout = Layout([100], fill_frame=True)
        self.add_layout(layout)


        #create a listbox to display results
        self._results_list = ListBox(
            Widget.FILL_FRAME,
            [],
            name="resultsList",
            add_scroll_bar=True
        )
        layout.add_widget(self._results_list)


        #buttons
        layout2 = Layout([1,1,1])
        self.add_layout(layout2)
        layout2.add_widget(Button("Display All Contacts", self._display_all), 0)
        layout2.add_widget(Button("Find Contacts Born in June", self._find_june), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        self.fix()
    
    def _display_all(self):
        global db_connection
        if db_connection is None:
            self._results_list.options = ["Error: DB connection not established"]
            self._results_list.update(0)
            return
        
        #run the query and fetch rows
        cursor = db_connection.cursor()
        cursor.execute("""
            SELECT c.name, c.birthday, c.anniversary, f.file_name
            FROM CONTACT c JOIN FILE f ON c.file_id = f.file_id
            ORDER BY c.name;
        """)
        results = cursor.fetchall()
        cursor.close()

        #build a list of lines for display
        rows = []

        #add a header
        rows.append(("Name         | Birthday     | Anniversary  | File", None))
        rows.append(("-------------|--------------|--------------|----------------", None))

        #widths for formatting
        col_name_width = 12
        col_bday_width = 20
        col_anniv_width = 20
        col_file_width = 16


        for (name, bday, anniv, filename) in results:
            
            
            #handle bday
            if bday is None:
                bday_str = "N/A"
            elif isinstance(bday, datetime.datetime):
                bday_str = bday.strftime("%Y-%m-%d %H:%M:%S")
            else:
                bday_str = str(bday)
            

            #handle anniv
            if anniv is None:
                anniv_str = "N/A"
            elif isinstance(anniv, datetime.datetime):
                anniv_str = anniv.strftime("%Y-%m-%d %H:%M:%S")
            else:  
                anniv_str = str(anniv)
            

            #handle filename, just incase, even though there needs to be one
            if not filename:
                filename = "N/A"
            
            
            #possibly make shorter
            if len(name) > col_name_width:
                name = name[:col_name_width-1] + "..."
            if len(bday_str) > col_bday_width:
                bday_str = bday_str[:col_bday_width-1] + "..."
            if len(anniv_str) > col_anniv_width:
                anniv_str = anniv_str[:col_anniv_width-1] + "..."
            if len(filename) > col_file_width:
                filename = filename[:col_file_width-1] + "..."
            

            #build the line
            line = (
                f"{name:<{col_name_width}} | "
                f"{bday_str:<{col_bday_width}} | "
                f"{anniv_str:<{col_anniv_width}} | "
                f"{filename}")

            #add to the list
            rows.append((line, None))

        #assign to the listbox
        self._results_list.options = rows

        #force update
        self._results_list.update(0)
        self.save()
    


    def _find_june(self):
        global db_connection
        if db_connection is None:
            self._results_list.options = ["Error: DB connection not established"]
            self._results_list.update(0)
            return
        
        cursor = db_connection.cursor()
        cursor.execute("""
            SELECT name, birthday FROM CONTACT
            WHERE MONTH(birthday) = 6
            ORDER BY birthday
        """)
        results = cursor.fetchall()
        cursor.close()

        rows = []
        rows.append(("Name         | Birthday", None))
        rows.append(("-------------|--------------", None))

        for (name, bday) in results:
            if not bday:
                bday_str = "N/A"
            else:
                bday_str = bday.strftime("%Y-%m-%d")
            line = f"{name:<10} | {bday_str}"
            rows.append((line, None))
        
        self._results_list.options = rows
        self._results_list.update(0)
        self.save()

    def _cancel(self):
        raise NextScene("Main")


#the login view for the DB
class LoginView(Frame):
    def __init__(self, screen):
        super(LoginView, self).__init__(screen,
                                        screen.height * 2 // 3,
                                        screen.width * 2 // 3,
                                        title="DB Login",
                                        hover_focus=True,
                                        can_scroll=False)
        #layout for fields
        layout = Layout([100])
        self.add_layout(layout)
        layout.add_widget(Text("Username:", "username"))
        layout.add_widget(Text("Password:", "password", hide_char="*"))
        layout.add_widget(Text("Database Name:", "dbname"))
        layout2 = Layout([1,1])
        self.add_layout(layout2)
        layout2.add_widget(Button("OK", self._ok), 0)
        layout2.add_widget(Button("Cancel", self._cancel), 1)
        self.fix()
    
    def _ok(self):
        self.save()
        #call the function to connect to the DB with these credentials
        username = self.data["username"]
        password = self.data["password"]
        dbname = self.data["dbname"]


        try:
            #try to connect
            global db_connection
            db_connection = connect_to_db(username, password, dbname)
            #create the tables
            create_tables(db_connection)
            print("Connected to DB")
            raise NextScene("Main")
        except mysql.connector.Error as err:
            if err.errno == 1045:
                message = "Error: Invalid username or password"
            elif err.errno == 1049:
                message = "Error: Invalid database name"
            else:
                message = f"Error: {err.msg}"
            
            print(message)
            

    def _cancel(self):
        raise StopApplication("User pressed Cancel")


#step 4: Main
def demo(screen, scene):
    model = VCardModel(folder=os.path.join(os.path.dirname(__file__), "cards"))
    scenes =[
        Scene([LoginView(screen)], -1, name="Login"),
        Scene([ListView(screen, model)], -1, name="Main"),
        Scene([DetailsView(screen, model)], -1, name="Details"),
        Scene([DBQueryView(screen)], -1, name="DBQueries")
    ]
    screen.play(scenes, stop_on_resize=True, start_scene=scene, allow_int=True)

last_scene = None
while True:
    try:
        Screen.wrapper(demo, catch_interrupt=True, arguments=[last_scene])
        sys.exit(0)
    except ResizeScreenError as e:
        last_scene = e.scene
                          
    

