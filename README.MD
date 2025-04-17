### vCard Manager ###

A text-based vCard viewer, editor, and database importer built with Python, C, and Asciimatics.

### FEATURES ### 

Scan & Validate: Scans the cards/ directory for valid vCard files using a C parser.

Terminal UI: Interactive list and detail views powered by Asciimatics.

Create & Edit: Create new vCards and edit the FN (Full Name) property.

Database Integration: Imports contact data into a MySQL database and provides built-in queries.

Requirements

Python 3.8+

asciimatics (terminal UI)

mysql-connector-python (MySQL integration)

A MySQL/MariaDB server with credentials

C compiler (to build libvcparser.so)

### INSTALLATION ###

Clone the Repository

git clone 
cd vcard-manager

Build the C Parser Library

cd bin
make  # Produces libvcparser.so
cd ..

Install Python Dependencies

pip install -r requirements.txt

### USAGE ###

Ensure a cards/ folder exists alongside A3Main.py and contains your .vcf/.vcard files.

Run the application:

python3 bin/A3Main.py

Log in with your MySQL credentials.

Browse, create, and edit vCards via the terminal UI.

Use the DB queries menu to view or import contacts into the database.

### DATABASE QUERIES ###

The app creates (if missing) two tables in your database:

CREATE TABLE FILE (
  file_id INT AUTO_INCREMENT PRIMARY KEY,
  file_name VARCHAR(60) NOT NULL,
  last_modified DATETIME,
  creation_time DATETIME NOT NULL
);

CREATE TABLE CONTACT (
  contact_id INT AUTO_INCREMENT PRIMARY KEY,
  name VARCHAR(256) NOT NULL,
  birthday DATETIME,
  anniversary DATETIME,
  file_id INT NOT NULL,
  FOREIGN KEY (file_id) REFERENCES FILE(file_id) ON DELETE CASCADE
);

### TECHNOLOGIES ###

Python 3 – Main application logic

C – vCard parsing in libvcparser.so

Asciimatics – Rich terminal-based UI

MySQL/MariaDB – Backend storage

ctypes – Python↔C interoperation






