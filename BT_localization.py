import os
import sys
import time
import struct
import operator
import bluetooth._bluetooth as bluez
from math import sqrt
from operator import itemgetter

test_bed_length = 200 #in cm
test_bed_width = 150 #in cm
beacon_BT_addr = ['70:3E:AC:75:19:26', '34:C8:03:89:8C:67', '80:EA:96:7D:A3:69'] #bsj, ray, gigantic leg
beacon_rssi_d0 = [-55.363, -57.8, -59.4]
beacon_loss_index = [3.348787454, 3.49622736, 2.654676987]
beacon_location = [(0,0),(0, test_bed_width),(test_bed_length,0)]
Epsilon = 5 #in cm
base = 50 #in cm
num_max = 3

# Determines whether two circles collide and, if applicable,
# the points at which their borders intersect.
# Based on an algorithm described by Paul Bourke:
# http://local.wasp.uwa.edu.au/~pbourke/geometry/2circle/
# Arguments:
#   P0 (tuple): the centre point of the first circle
#   P1 (tuple): the centre point of the second circle
#   r0 (numeric): radius of the first circle
#   r1 (numeric): radius of the second circle
# Returns:
#   False if the circles do not collide
#   True if one circle wholly contains another such that the borders
#       do not overlap, or overlap exactly (e.g. two identical circles)
#   An array of two tuple numbers containing the intersection points
#       if the circle's borders intersect.
def FindIntersectPoints(P0, P1, r0, r1):
    if type(P0) != tuple or type(P1) != tuple:
        raise TypeError("P0 and P1 must be tuple types")
    P0 = complex(P0[0], P0[1])
    P1 = complex(P1[0], P1[1])
    # d = distance
    d = sqrt((P1.real - P0.real)**2 + (P1.imag - P0.imag)**2)
    # n**2 in Python means "n to the power of 2"
    # note: d = a + b

    if d > (r0 + r1):
        return False
    elif d < abs(r0 - r1):
        return True
    elif d == 0:
        return True
    else:
        a = (r0**2 - r1**2 + d**2) / (2 * d)
        b = d - a
        h = sqrt(r0**2 - a**2)
        P2 = P0 + a * (P1 - P0) / d

        i1x = P2.real + h * (P1.imag - P0.imag) / d
        i1y = P2.imag - h * (P1.real - P0.real) / d
        i2x = P2.real - h * (P1.imag - P0.imag) / d
        i2y = P2.imag + h * (P1.real - P0.real) / d

        i1 = (i1x, i1y)
        i2 = (i2x, i2y)

        return [i1, i2]



def printpacket(pkt):
    for c in pkt:
        sys.stdout.write("%02x " % struct.unpack("B",c)[0])
    print 


def read_inquiry_mode(sock):
    """returns the current mode, or -1 on failure"""
    # save current filter
    old_filter = sock.getsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, 14)

    # Setup socket filter to receive only events related to the
    # read_inquiry_mode command
    flt = bluez.hci_filter_new()
    opcode = bluez.cmd_opcode_pack(bluez.OGF_HOST_CTL, 
            bluez.OCF_READ_INQUIRY_MODE)
    bluez.hci_filter_set_ptype(flt, bluez.HCI_EVENT_PKT)
    bluez.hci_filter_set_event(flt, bluez.EVT_CMD_COMPLETE);
    bluez.hci_filter_set_opcode(flt, opcode)
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, flt )

    # first read the current inquiry mode.
    bluez.hci_send_cmd(sock, bluez.OGF_HOST_CTL, 
            bluez.OCF_READ_INQUIRY_MODE )

    pkt = sock.recv(255)

    status,mode = struct.unpack("xxxxxxBB", pkt)
    if status != 0: mode = -1

    # restore old filter
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, old_filter )
    return mode

def write_inquiry_mode(sock, mode):
    """returns 0 on success, -1 on failure"""
    # save current filter
    old_filter = sock.getsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, 14)

    # Setup socket filter to receive only events related to the
    # write_inquiry_mode command
    flt = bluez.hci_filter_new()
    opcode = bluez.cmd_opcode_pack(bluez.OGF_HOST_CTL, 
            bluez.OCF_WRITE_INQUIRY_MODE)
    bluez.hci_filter_set_ptype(flt, bluez.HCI_EVENT_PKT)
    bluez.hci_filter_set_event(flt, bluez.EVT_CMD_COMPLETE);
    bluez.hci_filter_set_opcode(flt, opcode)
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, flt )

    # send the command!
    bluez.hci_send_cmd(sock, bluez.OGF_HOST_CTL, 
            bluez.OCF_WRITE_INQUIRY_MODE, struct.pack("B", mode) )

    pkt = sock.recv(255)

    status = struct.unpack("xxxxxxB", pkt)[0]

    # restore old filter
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, old_filter )
    if status != 0: return -1
    return 0

def device_inquiry_with_with_rssi(sock):
    # save current filter
    old_filter = sock.getsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, 14)

    # perform a device inquiry on bluetooth device #0
    # The inquiry should last 8 * 1.28 = 10.24 seconds
    # before the inquiry is performed, bluez should flush its cache of
    # previously discovered devices
    flt = bluez.hci_filter_new()
    bluez.hci_filter_all_events(flt)
    bluez.hci_filter_set_ptype(flt, bluez.HCI_EVENT_PKT)
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, flt )

    duration = 4
    max_responses = 255
    cmd_pkt = struct.pack("BBBBB", 0x33, 0x8b, 0x9e, duration, max_responses)
    bluez.hci_send_cmd(sock, bluez.OGF_LINK_CTL, bluez.OCF_INQUIRY, cmd_pkt)

    results = {}

    done = False
    while not done:
        pkt = sock.recv(255)
        ptype, event, plen = struct.unpack("BBB", pkt[:3])
        if event == bluez.EVT_INQUIRY_RESULT_WITH_RSSI:
            pkt = pkt[3:]
            nrsp = struct.unpack("B", pkt[0])[0]
            for i in range(nrsp):
                addr = bluez.ba2str( pkt[1+6*i:1+6*i+6] )
                if addr not in beacon_BT_addr:
                    continue;
                else:
                    rssi = struct.unpack("b", pkt[1+13*nrsp+i])[0]
                    results[addr] =  rssi
                # print "[%s] RSSI: [%d]" % (addr, rssi)
        elif event == bluez.EVT_INQUIRY_COMPLETE:
            done = True
        elif event == bluez.EVT_CMD_STATUS:
            status, ncmd, opcode = struct.unpack("BBH", pkt[3:7])
            if status != 0:
                print "uh oh..."
                printpacket(pkt[3:7])
                done = True
        elif event == bluez.EVT_INQUIRY_RESULT: 
            pkt = pkt[3:]
            nrsp = struct.unpack("B", pkt[0])[0]
            for i in range(nrsp):
                addr = bluez.ba2str( pkt[1+6*i:1+6*i+6] )
                results[addr] = -1 
                # print "[%s] (no RRSI)" % addr
        # else:
        #     print "unrecognized packet type 0x%02x" % ptype
    	   #  print "event ", event


    # restore old filter
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, old_filter )

    return results

def localization():
    dev_id = 0
    try:
        sock = bluez.hci_open_dev(dev_id)
    except:
        print "error accessing bluetooth device..."
        sys.exit(1)

    try:
        mode = read_inquiry_mode(sock)
    except Exception, e:
        print "error reading inquiry mode.  "
        print "Are you sure this a bluetooth 1.2 device?"
        print e
        sys.exit(1)
    print "current inquiry mode is %d" % mode
    if mode != 1:
        print "writing inquiry mode..."
        try:
            result = write_inquiry_mode(sock, 1)
        except Exception, e:
            print "error writing inquiry mode.  Are you sure you're root?"
            print e
            sys.exit(1)
        if result != 0:
            print "error while setting inquiry mode"
        print "result: %d" % result

    print "reading beacon rssi..."
    results = {}
    for key in beacon_BT_addr:
        results[key] = [];
    for i in range(5):
        time.sleep(0.1)
        rssi = device_inquiry_with_with_rssi(sock)
        print "rssi = ", rssi
        for key in rssi:
            results[key].append(rssi[key])
    print "results=", results


    print "calculate average rssi..."
    print "num_max=",num_max
    for key in results:
        if results[key]:
            temp = 0;
            results[key].sort()
            for i in range(num_max):
                temp += results[key][i]
            temp /= num_max
            results[key] = temp
        else:
            results[key] = -300
    print "average rssi = ", results

    print "calculate distance..."
    for key in results:
        index = beacon_BT_addr.index(key)
        d = pow(10, (beacon_rssi_d0[index] - results[key]) / (10*beacon_loss_index[index]))
        d *= base
        print "distance from beacon %s is %d" % (key, d)
        results[key] = d


    print "finding location..."
    sorted_distance = sorted(results.items(), key=operator.itemgetter(1))
    print "sorted_distance = ", sorted_distance
    sorted_distance = sorted_distance[:2]
    print "sorted_distance = ", sorted_distance
    (addr, d1) = sorted_distance[0]
    index = beacon_BT_addr.index(addr)
    center1 = beacon_location[index] #d, center of circle for addr1
    (addr, d2) = sorted_distance[1]
    index = beacon_BT_addr.index(addr)
    center2 = beacon_location[index] #d, center of circle for addr2

    #find all intersectin points 
    intersection_pts = []
    for n in range(4):
        intersection_pt = FindIntersectPoints(center1, center2, d1+(-1)**n*Epsilon, d2+(-1)**(n/2)*Epsilon)
        print "intersection_pt = ", intersection_pt
        if type(intersection_pt) == list:
            intersection_pts += intersection_pt
    print "all intersection_pts are ", intersection_pts

    #remove all intersection pts outside of test bed
    for (x, y) in intersection_pts:
        if x > test_bed_length or x < 0:
            intersection_pts.remove((x,y))
        if y > test_bed_width or y < 0:
            intersection_pts.remove((x,y))
    print "all intersection_pts remaining are ", intersection_pts
    #only keep points with max/min xs and ys
    quadrilateral = []
    for i in range(2):
        if intersection_pts:
            max_point = max(intersection_pts, key=itemgetter(i))
            quadrilateral.append(max_point)
            intersection_pts.remove(max_point)
        if intersection_pts:
            min_point = min(intersection_pts, key=itemgetter(i))
            quadrilateral.append(min_point)
            intersection_pts.remove(min_point)
    print "quadrilateral = ", quadrilateral

    if not quadrilateral:
        print "no intersection pts"
        return

    #find barycenter of quadrilateral
    sum_x = 0
    sum_y = 0
    for (x,y) in quadrilateral:
        sum_x += x
        sum_y += y
    x_loc = sum_x/len(quadrilateral)
    y_loc = sum_y/len(quadrilateral)
    print "x_loc=%s, y_loc=%s" % (x_loc,y_loc)
    return str(x_loc)+","+str(y_loc)

# localization()