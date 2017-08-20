# Self-healing pyserial with GUI; received characters 'bunching' effect handled.
# use it with occupancySimul2
# python -m serial.tools.list_ports
# python -m serial.tools.miniterm  COM1

import serial
import threading
from time import sleep
from Tkinter import *
from PyQt4 import QtCore 
from PIL import ImageTk, Image

port = 'COM7'
baud = 9600
serialdata = "dummy"
terminate = False
daemonalive = False

class SensorThread(threading.Thread):
    def run(self):
        global serialdata, daemonalive  
        daemonalive = True
        print 'Daemon starts\n' 
        ser = None
        try:        
            ser = serial.Serial(port, baud, timeout=0)
            ser.flushInput()        
            while not terminate:            
                if ser.inWaiting():
                    serialdata = ser.readline()
                    print serialdata,' '
        except Exception as e:
            print "*(S)", e   
            sleep(1)
        finally:
            if ser is not None:
                ser.flushOutput()
                ser.close()
            daemonalive = False   # to signal the main thread
            print 'Daemon exits\n'       
#--------------------------------------------------------------------

class MonitorThread(threading.Thread):
    def run(self):
        while not terminate:
            if (daemonalive):
                sleep(1)
            else:
                self.startDaemon()
 
    def startDaemon(self):
        global terminate   # this is absolutely necessary !
        terminate = False
        th = SensorThread()
        th.setDaemon(True)
        th.start()
    
    def stopDaemon(self):
        global terminate   # this is absolutely necessary !
        terminate = True
        sleep(2)  # enable serial to time out

#--------------------------------------------------------------------
class Gui(object):
    def __init__(self):
        self.root = Tk()
        self.redbulb = ImageTk.PhotoImage(Image.open("red.jpg"))
        self.greenbulb = ImageTk.PhotoImage(Image.open("green.jpg"))
        self.offbulb = ImageTk.PhotoImage(Image.open("off.jpg"))
        self.lbl = Label(self.root, image=self.offbulb)
        self.root.minsize(width=300, height=170)
        self.status = -1      # invalid initial stauses
        self.prevstatus = -1
        self.lbl.pack()
        self.root.pack_propagate(0)
        
    def prime(self):
        self.readSensor()
        self.root.mainloop()

    def writeTS(self):
        time = QtCore.QTime.currentTime()
        text = time.toString('hh:mm:ss')
        f.write (text +',')
        
    def readSensor(self):
        try:
            dat = serialdata.strip()    # remove newline chars 
            #print dat
            self.status = int(dat)%10   # take only the last digit (latest status)    
            if (self.status != self.prevstatus): 
                 self.prevstatus = self.status
                 if (self.status==1):
                     self.lbl['image'] = self.redbulb
                 else:
                     self.lbl['image'] = self.greenbulb
                 self.root.update()
                 self.writeTS()
                 #f.write (dat +'\n')
                 f.write (str(self.status) +'\n')
        except Exception as e:
            print "*(G)", e
        finally:
            self.root.after(50, self.readSensor)

#--------------------------------------------------------------------
        
print 'Press ^C to quit...'    
 
f = open("log1.csv", "w")
print 'Log file opened'

print 'Outer loop begins'
th = MonitorThread()
th.setDaemon(True)
th.start()

Gui().prime()

th.stopDaemon() 
f.close()
print 'Log file closed'
print 'Main thread exits.' 

