# After cleanup: self-healing pyserial
# Read serial port; if the link fails, re-establish connection.
# use it with occupancySimul2

import serial
import threading
from time import sleep

port = 'COM7'
baud = 9600
serialdata = "dummy"
terminate = False
daemonalive = True

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
            print "*(1)", e   
            sleep(1)
        finally:
            if ser is not None:
                ser.flushOutput()
                ser.close()
            daemonalive = False   # to signal the main thread
            print 'Daemon exits\n'       
#--------------------------------------------------------------------

def startDaemon():
    global terminate  # this is absolutely necessary !
    terminate = False
    th = SensorThread()
    th.setDaemon(True)
    th.start()

def stopDaemon():
    global terminate # this is absolutely necessary !
    terminate = True
    sleep(2)  # enable serial to time out
    
def processData(): 
    global serialdata
    if serialdata=='5':
        serialdata = "dummy" # * cludge: avoid repeatedly reading the last value *
        print ' :Your father lies under fathom five'
    
def initFile():
    print 'Log file opened'
        
def closeFile():
    print 'Log file closed'
#--------------------------------------------------------------------
        
print 'Press ^C to quit...'    
print 'Outer loop begins'
initFile();
startDaemon()

while True:
    try:
        print 'Inner loop begins'
        while daemonalive:
            processData()
        print 'Inner loop ends'
        startDaemon()    
    except KeyboardInterrupt:
        print '^C detected !'
        break
print 'Outer loop ends'
    
stopDaemon() 
closeFile()
print 'Main thread exits.' 

