#!/usr/bin/python3

import unittest
import os
import stat
import io
import subprocess
import inspect
import shutil
import sh
from sh import ls, mkdir, rm, cd, touch, cat, echo, seq, rmdir
from time import sleep

USER_ID = os.getuid()
GROUP_ID = os.getgid()
TEST_MOUNT_DIR = "prueba"
IMAGE_FILENAME = "image.fisopfs"
SUDO_PASSWORD = os.getenv("SUDO_PASSWORD")

def make_path(path: str):
    return os.path.join(TEST_MOUNT_DIR, path)
    
def get_full_path(path: str):
    return(os.path.abspath(path))

class TestFisopfs(unittest.TestCase):

    def test_after_creating_an_empty_file_its_stats_are_correct(self):
        touch(make_path("hello.txt"))
        file_stat = os.stat(make_path("hello.txt"))
        
        self.assertEqual(file_stat.st_size, 0)
        self.assertEqual(file_stat.st_blocks, 0)
        self.assertNotEqual(file_stat.st_mode & stat.S_IFREG, 0)
        self.assertEqual(file_stat.st_uid, USER_ID)
        self.assertEqual(file_stat.st_gid, GROUP_ID)
    

    def test_reading_an_empty_file_returns_nothing(self):
        touch(make_path("hello.txt"))
        with io.StringIO() as out:
            cat(make_path("hello.txt"), _out=out)
            outval = out.getvalue()

            self.assertEqual(outval, "")
        

    def test_writing_an_empty_file_changes_its_stats(self):
        filename = "hello.txt"
        filepath = make_path(filename)
        test_string = "Hello World!"
        
        touch(filepath)
        with open(filepath, "w") as hello:
            echo(test_string, _out=hello)
        file_stat = os.stat(filepath)

        self.assertEqual(file_stat.st_size, len(test_string + "\n"))
        self.assertEqual(file_stat.st_blocks, 1)
            
            
    def test_writing_a_file_and_reading_it_shows_its_contents_correctly(self):
        filename = "hello.txt"
        filepath = make_path(filename)
        test_string = "Hello World!"
        
        touch(filepath)
        with open(filepath, "w") as hello:
            echo(test_string, _out=hello)
        with io.StringIO() as out:
            cat(filepath, _out=out)
            outval = out.getvalue()
            self.assertEqual(outval, test_string + "\n")
            
    def test_writing_a_file_appending_to_it_and_then_reading_it_shows_its_contents_correctly(self):
        filename = "hola.txt"
        filepath = make_path(filename)
        test_string1 = "primera línea"
        test_string2 = "una segunda línea"
        test_string3 = "última línea"
        
        touch(filepath)
        with open(filepath, "a") as hello:
            echo(test_string1, _out=hello)
            echo(test_string2, _out=hello)
            echo(test_string3, _out=hello)
            # obs: ^ esto en bash es appendear usando echo con '>>', funciona correctamente.
            # (ver pruebas en archivo .sh)
        with io.StringIO() as out:
            cat(filepath, _out=out)
            outval = out.getvalue()
            self.assertEqual(outval, test_string1 + "\n" + test_string2 + "\n" + test_string3 + "\n")       
       
    def test_writing_a_file_of_many_blocks_and_reading_it_shows_its_contents_correctly(self):
        filename = "seq.txt"
        filepath = make_path(filename)
        s = "\n".join([str(i) for i in range(1, 5000+1)])
        
        with open(filepath, "w") as seqfile:
            seq("5000", _out=seqfile)
        with io.StringIO() as out:
            cat(filepath, _out=out)
            outval = out.getvalue()
            self.assertEqual(outval, s + "\n")
        
    
    def test_after_creating_a_file_it_gets_listed_in_the_cwd(self):
        filename = "somefile.txt"
        filepath = make_path(filename)
        touch(filepath)
        with io.StringIO() as out:
            ls(TEST_MOUNT_DIR, _out=out)
            self.assertTrue(filename in out.getvalue())
            

    def test_after_removing_a_file_it_no_longer_gets_listed_in_the_cwd(self):
        filename = "somefile.txt"
        filepath = make_path(filename)
        rm(filepath)
        with io.StringIO() as out:
            ls(TEST_MOUNT_DIR, _out=out)
            self.assertTrue(filename not in out.getvalue())

    def test_after_creating_a_dir_it_gets_listed_in_the_cwd(self):
    	dirname = "unDir"
    	dirpath = make_path(dirname)
    	mkdir(dirpath)
    	with io.StringIO() as out:
    		ls(TEST_MOUNT_DIR, _out=out)
    		self.assertTrue(dirname in out.getvalue())

    def test_after_removing_a_dir_it_no_longer_gets_listed_in_the_cwd(self):
    	dirname = "unDir"
    	dirpath = make_path(dirname)
    	rmdir(dirpath)
    	with io.StringIO() as out:
    		ls(TEST_MOUNT_DIR, _out=out)
    		self.assertTrue(dirname not in out.getvalue())

if __name__ == "__main__":
    # setup test suite
    suite = unittest.TestSuite()
    
    is_test_function = lambda member: inspect.isfunction(member) and member.__name__.startswith("test")
    
     # list of tuples (name, testmethod)
    test_methods = list(inspect.getmembers(TestFisopfs, is_test_function))
    
    # sort by line number: run tests in the order in which they are declared
    test_methods = sorted(test_methods, key=lambda tup: inspect.getsourcelines(tup[1])[1])
    print("\n".join([tup[0] for tup in test_methods]))
    
    suite.addTests([TestFisopfs(tup[0]) for tup in test_methods])
    
    try:
        if os.path.exists(IMAGE_FILENAME):
            os.unlink(IMAGE_FILENAME)
        try:
            os.mkdir(TEST_MOUNT_DIR)
        except:
            os.rmdir(TEST_MOUNT_DIR)
            os.mkdir(TEST_MOUNT_DIR)

        with open("logfile.log", "w") as logfile:
            with subprocess.Popen(["./fisopfs", "-f", TEST_MOUNT_DIR], stdout=logfile) as proc:
                runner = unittest.TextTestRunner()
                runner.run(suite)

                if os.path.ismount(get_full_path(TEST_MOUNT_DIR)):
                    sh.contrib.sudo.umount(get_full_path(TEST_MOUNT_DIR), password=SUDO_PASSWORD)
   
    finally:
        if not os.path.ismount(get_full_path(TEST_MOUNT_DIR)):
            try:
                shutil.rmtree(get_full_path(TEST_MOUNT_DIR))
            except:
                os.rmdir(TEST_MOUNT_DIR)
            print("The FS was unmounted [OK]")
        else:
            print("unmount [ERROR]")
