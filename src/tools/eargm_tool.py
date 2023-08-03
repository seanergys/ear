import re

print("")
print("=========================== This tool is used to automatically build the EARGM configuration ===========================")
print("")

print("=======================")
print("= EARGM Configuration =")
print("=======================")
print("")

EARGMUseAggregated = input("Do you want to use EAR aggregated periodic metrics, \n(Aggregated metrics are only available when EARDBD is running) [Y/n]: ")
if EARGMUseAggregated == '':
	EARGMUseAggregated = 'y'
while EARGMUseAggregated != 'y' and EARGMUseAggregated != 'Y' and EARGMUseAggregated != 'n' and EARGMUseAggregated != 'N':
	if EARGMUseAggregated == '':
		EARGMUseAggregated = 'y'
		break
	EARGMUseAggregated = input("[Y/n]: ")
if EARGMUseAggregated == 'y' or EARGMUseAggregated == 'Y':
	EARGMUseAggregated = 1
if EARGMUseAggregated == 'n' or EARGMUseAggregated == 'N':
	EARGMUseAggregated = 0
print("")
'''
EARGMMode = input("Which mode do you want to use? Two modes are supported '0=manual' and '1=automatic': ")
while EARGMMode == '' or not(EARGMMode.strip().isdigit()) or (int(EARGMMode) != 0 and int(EARGMMode) != 1):
        EARGMMode = input("Which mode do you want to use? Two modes are supported '0=manual' and '1=automatic': ")
print("")
'''
EARGMUnits = input("Enter the unit that you want to use? '-' are Joules, 'K' KiloJoules and 'M' MegaJoules: ")
while EARGMUnits != '-' and EARGMUnits != 'K' and EARGMUnits != 'k' and EARGMUnits != 'M' and EARGMUnits != 'm':
        #EARGMUnits = input("Enter the unit that you want to use? '-' are Joules, 'K' KiloJoules and 'M' MegaJoules: ")
	EARGMUnits = input("[-/K/M]")
if EARGMUnits == '-':
        EARGMUnits ='J'
if EARGMUnits == 'k':
        EARGMUnits ='K'
if EARGMUnits == 'm':
        EARGMUnits ='M'
print("")

#try:
EARGMWarningsPerc = input("Enter the level L4, L3 and L2 percentage of accumulated energy to start the warning \nEnter three values from lowest to highest, comma separated format (e.g. 85, 90, 95): ")
pattern = re.compile(r"^(\w+)(,\s*\w+)*$")
flag = 0
i = 1
numbers = [int(x.strip()) for x in EARGMWarningsPerc.split(',')]
while i < len(numbers):
        if(numbers[i] < numbers[i - 1]):
                flag = 1
        i += 1
while EARGMWarningsPerc == '' or pattern.match(EARGMWarningsPerc) == None or (len([int(x.strip()) for x in EARGMWarningsPerc.split(',')]) != 3) or flag > 0:
        EARGMWarningsPerc = input("Enter three values from lowest to highest, comma separated format (e.g. 85, 90, 95): ")
#except ValueError:
#       print("The used format was incorrect")
#except:
#       print("There was an error with setting the EARGMWarningsPerc")
print("")

print("Period T1 and T2 are specified in seconds (T1 must be less than T2, ex. 10min and 1 month)")
T1 = input("Enter the Period T1 in seconds: ")
while T1 == '':
	#print("You entered invalid value!!\n")
	T1 = input("Enter the Period T1 in seconds: ")

T2 = input("Enter the Period T2 in seconds: ")
while T2 == '' or int(T2) < int(T1):
        #print("You entered invalid value!!\n")
        T2 = input("Enter the Period T2 in seconds, T2 must be greater than T1: ")
print("")

EARGMGracePeriods = input("T1 \"grace\" periods between DEFCON before re-evaluate: ")
while EARGMGracePeriods == '' or not(EARGMGracePeriods.strip().isdigit()):
	EARGMGracePeriods = input("T1 \"grace\" periods between DEFCON before re-evaluate: ")
print("")

EARGMEnergyAction = input("You can configure a command to automatically executed at each warning level (only once per grace periods),\nDo you want to execute a command [y/N]: ")
if EARGMEnergyAction == 'Y' or EARGMEnergyAction == 'y':
        EARGMEnergyAction = input("Enter the path to the command: ")
while EARGMEnergyAction == '' or EARGMEnergyAction.strip().isdigit():
		EARGMEnergyAction = input("Enter a valid path to the command: ")
if EARGMEnergyAction == '' or EARGMEnergyAction == 'N' or EARGMEnergyAction == 'n':
        EARGMEnergyAction = "no_action"
print("")

EARGMUseLog = input("Do you want to generate a log file in 'TmpDir'/eargmd.log [Y/n]: ")
if EARGMUseLog == '':
	EARGMUseLog = 'y'
while EARGMUseLog != 'y' and EARGMUseLog != 'Y' and EARGMUseLog != 'n' and EARGMUseLog != 'N':
	if EARGMUseLog == '':
                EARGMUseLog = 'y'
                break
	EARGMUseLog = input("[Y/n]: ")
if EARGMUseLog == 'y' or EARGMUseLog == 'Y':
        EARGMUseLog = 1
if EARGMUseLog == 'n' or EARGMUseLog == 'N':
        EARGMUseLog = 0
print("")

EARGMMail = input("Do you want to recieve emails about the warnings [y/N]: ")
if EARGMMail == 'Y' or EARGMMail == 'y':
        EARGMMail = input("Enter an email address: ")
        if EARGMMail == '' or not(EARGMMail.strip().isdigit()):
                EARGMMail = input("Enter a valid email address: ")
if EARGMMail == '' or EARGMMail == 'N' or EARGMMail == 'n':
        EARGMMail = 'nomail'
print("")

print("==========================")
print("= POWERCAP Configuration =")
print("==========================")
print("")

EARGMPowerPeriod = input("Enter the time period at which the powercap thread is activated\n(Meta-EARGM checks the EARGMs it controls every 2*EARGMPowerPeriod) [in seconds]: ")
while not(EARGMPowerPeriod.strip().isdigit()) or EARGMPowerPeriod == '':
	EARGMPowerPeriod = input("Enter the time period in seconds: ")
print("")

EARGMPowerCapMode = input("Enter the powercap mode (0 is monitoring, 1 is hard powercap, 2 is soft powercap) [0/1/2]: ")
while not(EARGMPowerCapMode.strip().isdigit()) or EARGMPowerCapMode == '' or (int(EARGMPowerCapMode) != 0 and int(EARGMPowerCapMode) != 1 and int(EARGMPowerCapMode) != 2 ):
	EARGMPowerCapMode = input("[0/1/2]: ")
print("")

EARGMPowerCapSuspendConfig = input("Do you want to automatically execute an action in EARGMPowerCapSuspendAction\nwhen the total_power >= EARGMPowerLimit*EARGMPowerCapResumeLimit/100 [y/N]: ")
while EARGMPowerCapSuspendConfig.strip().isdigit() or (EARGMPowerCapSuspendConfig != '' and EARGMPowerCapSuspendConfig != 'y' and EARGMPowerCapSuspendConfig != 'Y' and EARGMPowerCapSuspendConfig != 'n' and EARGMPowerCapSuspendConfig != 'N'):
	EARGMPowerCapSuspendConfig = input("Do you want to automatically execute an action in EARGMPowerCapSuspendAction [y/N]: ")
if EARGMPowerCapSuspendConfig == 'Y' or EARGMPowerCapSuspendConfig == 'y':
	EARGMPowerCapSuspendLimit = input("Enter the power percentage value: ")
	while not(EARGMPowerCapSuspendLimit.strip().isdigit()) or EARGMPowerCapSuspendLimit == '':
		EARGMPowerCapSuspendLimit = input("Enter a valid power percentage value: ")
	print("")
	EARGMPowerCapSuspendAction = input("Enter the path to the command: ")
	while EARGMPowerCapSuspendAction == '' or EARGMPowerCapSuspendAction.strip().isdigit():
		EARGMPowerCapSuspendAction = input("Enter a valid path to the command: ")
	print("")

if EARGMPowerCapSuspendConfig == 'N' or EARGMPowerCapSuspendConfig == 'n' or EARGMPowerCapSuspendConfig == '':
	EARGMPowerCapSuspendLimit = 0
	EARGMPowerCapSuspendAction = "no_action"
print("")

EARGMPowerCapResumeConfig = input("Do you want to automatically execute an action in EARGMPowerCapResumeAction to undo EARGMPowerCapSuspendAction\nwhen the total_power >= EARGMPowerLimit*EARGMPowerCapResumeLimit/100 [y/N]: ")
while EARGMPowerCapResumeConfig.strip().isdigit() or (EARGMPowerCapResumeConfig != '' and EARGMPowerCapResumeConfig != 'y' and EARGMPowerCapResumeConfig != 'Y' and EARGMPowerCapResumeConfig != 'n' and EARGMPowerCapResumeConfig != 'N'):
        EARGMPowerCapResumeConfig = input("Do you want to automatically execute an action in EARGMPowerCapResumeAction [y/N]: ")
if EARGMPowerCapResumeConfig == 'Y' or EARGMPowerCapResumeConfig == 'y':
        EARGMPowerCapResumeLimit = input("Enter the power percentage value: ")
        while not(EARGMPowerCapResumeLimit.strip().isdigit()) or EARGMPowerCapResumeLimit == '':
                EARGMPowerCapResumeLimit = input("Enter a valid power percentage value: ")
        print("")
        EARGMPowerCapResumeAction = input("Enter the path to the command: ")
        if EARGMPowerCapResumeAction == '' or EARGMPowerCapResumeAction.strip().isdigit():
                EARGMPowerCapResumeAction = input("Enter a valid path to the command: ")
        print("")

if EARGMPowerCapResumeConfig == 'N' or EARGMPowerCapResumeConfig == 'n' or EARGMPowerCapResumeConfig == '':
        EARGMPowerCapResumeLimit = 0
        EARGMPowerCapResumeAction = "no_action"
print("")

print("==================")
print("= Setting EARGMs =")
print("==================")
print("")

print("EARGMs must be specified with a unique node and port that receives remote connections.")
print("An EARGM can also act as meta-EARGM if the meta field is filled, and it will control the EARGMs \nwhose ids are in said field. If two EARGMs are in the same node setting the EARGMID environment variable \noverrides the node field and chooses the characteristics of the EARGM with the correspoding id.")
print("")
print("Only one EARGM can currently control the energy caps, so setting the rest to 0 is recommended.")
print("energy = 0 -> energy_cap disabled")
print("power = 0  -> powercap disabled")
print("power = N  -> powercap budget for that EARGM (and the nodes it controls) is N")
print("power = -1 -> powercap budget is calculated by adding up the powercap set to each of the nodes under its control.")
print("")

EARGMIdList = []
energyList = []
powerList = []
nodeList = []
portList = []
metaList = []
metaIdsList = []
newMetaList = []
node_indices = []
node_ports = []

print("")
print("Configure the first EARGM")

i = 1
print("")
energy = input("Enter the energy amount: ")
while not(energy.strip().isdigit()) or energy == '':
	energy = input("Enter the energy amount: ")

power = input("Enter the power amount: ")
#while not(power.strip().isdigit()) or power == '':
while power == '':
	power = input("Enter the power amount: ")

node = input("Enter the nodename: ")
while (node.strip().isdigit()) or node == '':
	node = input("Enter the nodename: ")

port = input("Enter the port number: ")
while not(port.strip().isdigit()) or port == '':
	port = input("Enter a valid port number: ")
print("")

EARGMIdList.append(i)
energyList.append(energy)
powerList.append(power)
nodeList.append(node)
portList.append(port)

addmore = input("Do you want to add/create EARGMs [Y/n]: ")
while addmore == 'Y' or addmore == 'y' or addmore == '':
	
	i += 1
	
	if any(int(x) > 0 for x in energyList):
		energy = 0
	else:
		energy = input("Enter the energy amount: ")
		while not(energy.strip().isdigit()) or energy == '':
			energy = input("Enter the energy amount: ")

	power = input("Enter the power amount: ")
	#while not(power.strip().isdigit()) or power == '':
	while power == '':
		power = input("Enter the power amount: ")

	node = input("Enter the nodename: ")
	while (node.strip().isdigit()) or node == '':
		node = input("Enter the nodename: ")

	if node in nodeList:

		node_indices  = [index for (index, item) in enumerate(nodeList) if item == node]
		node_ports = [item for (index, item) in enumerate(portList) if index in node_indices]
		print("The allowed ports for this node should not include:", '[%s]' % ', '.join(map(str, node_ports)))

	port = input("Enter the port number: ")
	while not(port.strip().isdigit()) or port == '' or port in node_ports:
		port = input("Enter a valid port number: ")
	print("")
	

	EARGMIdList.append(int(i))
	energyList.append(energy)
	powerList.append(power)
	nodeList.append(node)
	portList.append(port)

	addmore = input("Do you want to add more EARGMs [Y/n]: ")
	if addmore == 'Y' or addmore == 'y' or addmore == '':
		print("Adding a new EARGM")
		continue
	else:
		print("No more EARGMs will be configured")
		break
print("")

itr = 0
metaList = [[]] * len(EARGMIdList)
metaEargm = input("Do you want to configure any of the EARGMs as meta-EARGM [y/N]: ")
if metaEargm == '':
	metaEargm = 'N'
while (metaEargm.strip().isdigit()) or (metaEargm != 'y' and metaEargm != 'Y' and metaEargm != 'n' and metaEargm != 'N'):
	metaEargm = input("[y/N]: ")
	if metaEargm == '':
		metaEargm = 'N'
while metaEargm == 'y' or metaEargm == 'Y':
	print("")
	print("The list of the current configured EARGM's IDs is:", '[%s]' % ', '.join(map(str, EARGMIdList)))
	print("")
	meta = input("Enter the ID of the EARGM that you want to use as meta-EARGM: ")
	while meta == '' or not(meta.strip().isdigit()) or int(meta) not in EARGMIdList:
		meta = input("Enter a valid EARGM ID from the provided list of IDs: ")
	meta_idx = EARGMIdList.index(int(meta))
	print("")
	metaIds = input("Enter the EARGM's IDs from the provided list of IDs that will be controled by this meta-EARGM\n(comma separated format expected (e.g. 1, 2)): ")

	try:
		pattern = re.compile(r"^(\w+)(,\s*\w+)*$")
		while metaIds == '' or pattern.match(metaIds) == None:
			metaIds = input("Enter the EARGM's IDs from the provided list of IDs (comma separated format expected (e.g. 1, 2)): ")
	except ValueError:
		print("Enter a valid format")
		continue
	
	metaIdsList = metaIds.split(",")
	if all(int(x) in EARGMIdList for x in metaIdsList):	
		metaList[meta_idx] = metaIdsList
		for sublist in metaList:
			for item in sublist:
				newMetaList.append(item)
		if itr > 0:	
			if any(str(x) in newMetaList for x in metaIdsList):
				print("The EARGM ID should not be duplicated")
				#metaIds = input("Enter the EARGM's IDs from the provided list of IDs (comma separated format expected (e.g. 1, 2)): ")
				print("Need to start over the meta-EARGM configuration")
				continue
			else:
				print("meta-EARGM is configuered")
		else:
			print("meta-EARGM is configuered")
		itr += 1
	else:
		print("The provided list is not part of the current configured EARGMs")
		print("Need to start over the meta-EARGM configuration")
		continue

	'''
	newMetaList = []
	for sublist in metaList:
		for item in sublist:
			newMetaList.append(item)
	'''

	flag = len(EARGMIdList)
	for x in EARGMIdList:
		if (str(x) in newMetaList):
			flag -= 1

	if flag != 0:
		metaEargm = input("Do you want to configure another meta-EARGM [y/N]: ")
		if metaEargm == '':
			metaEargm = 'N'
		while (metaEargm.strip().isdigit()) or (metaEargm != 'y' and metaEargm != 'Y' and metaEargm != 'n' and metaEargm != 'N'):
			metaEargm = input("[y/N]: ")
			if metaEargm == '':
				metaEargm = 'N'
	else:
		print("Finished the meta-EARGM configuration")
		break	

if metaEargm == 'n' or metaEargm == 'N' or metaEargm == '':
	print("Finished the meta-EARGM configuration")
print("")

'''
for i in range(len(EARGMIdList)):
	if metaList[i] == []:
		print("EARGMId={} energy={} power={} node={} port={}".format(EARGMIdList[i], energyList[i], powerList[i], nodeList[i], portList[i]))
	else:
		print("EARGMId={} energy={} power={} node={} port={} meta={}".format(EARGMIdList[i], energyList[i], powerList[i], nodeList[i], portList[i], ','.join(metaList[i])))
'''

#Print Section:
print("")
print("")
print("#---------------------------------------------------------------------------------------------------")
print("# EAR Global Manager (EARGMD): Update that section to use EARGM.")
print("#---------------------------------------------------------------------------------------------------")
print("#")
print("# Use aggregated periodic metrics or periodic power metrics.")
print("# Aggregated metrics are only available when EARDBD is running.")
print("#")
print("EARGMUseAggregated=", EARGMUseAggregated, sep='')
print("# Period T1 and T2 are specified in seconds (ex. T1 must be less than T2, ex. 10min and 1 month).")
print("EARGMPeriodT1=" + T1)
print("EARGMPeriodT2=" + T2)
print("# '-' are Joules, 'K' KiloJoules and 'M' MegaJoules.")
print("EARGMUnits=", EARGMUnits, sep='')
print("EARGMPort=50000")
print("# Two modes are supported '0=manual' and '1=automatic'.")
print("EARGMMode=1")
print("# Email address to report the warning level (and the action taken in automatic mode).")
print("EARGMMail=", EARGMMail, sep='')
print("# Percentage of accumulated energy to start the warning DEFCON level L4, L3 and L2.")
print("EARGMWarningsPerc=", EARGMWarningsPerc.replace(" ",""), sep='')
print("# T1 \"grace\" periods between DEFCON before re-evaluate.")
print("EARGMGracePeriods=", EARGMGracePeriods, sep='')
print("# Verbosity")
print("EARGMVerbose=0")
print("# When set to 1, the output is saved in 'TmpDir'/eargmd.log (common configuration) as a log file.")
print("EARGMUseLog=", EARGMUseLog, sep='')
print("#")
print("# Format for action is: command_name energy_T1 energy_T2  energy_limit T2 T1  units")
print("# This action is automatically executed at each warning level (only once per grace periods).")
print("#")
print("EARGMEnergyAction=", EARGMEnergyAction, sep='')
print("# Period at which the powercap thread is activated. Meta-EARGM checks the EARGMs it controls every 2*EARGMPowerPeriod")
print("EARGMPowerPeriod=", EARGMPowerPeriod, sep='')
print("# Powercap mode: 0 is monitoring, 1 is hard powercap, 2 is soft powercap.")
print("EARGMPowerCapMode=", EARGMPowerCapMode, sep='')
print("# Admins can specify to automatically execute a command in EARGMPowerCapSuspendAction when total_power >= EARGMPowerLimit*EARGMPowerCapResumeLimit/100")
print("EARGMPowerCapSuspendLimit=", EARGMPowerCapSuspendLimit, sep='')
print("# Format for action is: command_name current_power current_limit total_idle_nodes total_idle_power")
print("EARGMPowerCapSuspendAction=", EARGMPowerCapSuspendAction, sep='')
print("#")
print("# Admins can specify to automatically execute a command in EARGMPowerCapResumeAction to undo EARGMPowerCapSuspendAction")
print("# when total_power >= EARGMPowerLimit*EARGMPowerCapResumeLimit/100.")
print("# Note that this will only be executed if a suspend action was executed previously.")
print("#")
print("EARGMPowerCapResumeLimit=", EARGMPowerCapResumeLimit, sep='')
print("# Format for action is: command_name current_power current_limit total_idle_nodes total_idle_power")
print("EARGMPowerCapResumeAction=", EARGMPowerCapResumeAction, sep='')
print("# EARGMs must be specified with a unique id, their node and the port that receives remote")
print("# connections. An EARGM can also act as meta-eargm if the meta field is filled, and it will")
print("# control the EARGMs whose ids are in said field. If two EARGMs are in the same node,")
print("# setting the EARGMID environment variable overrides the node field and chooses the characteristics")
print("# of the EARGM with the correspoding id.")
print("#")
print("# Only one EARGM can currently control the energy caps, so setting the rest to 0 is recommended.")
print("# energy = 0 -> energy_cap disabled")
print("# power = 0  -> powercap disabled")
print("# power = N  -> powercap budget for that EARGM (and the nodes it controls) is N")
print("# power = -1 -> powercap budget is calculated by adding up the powercap set to each of the nodes under its control.")

for i in range(len(EARGMIdList)):
        if metaList[i] == []:
                print("EARGMId={} energy={} power={} node={} port={}".format(EARGMIdList[i], energyList[i], powerList[i], nodeList[i], portList[i]))
        else:
                print("EARGMId={} energy={} power={} node={} port={} meta={}".format(EARGMIdList[i], energyList[i], powerList[i], nodeList[i], portList[i], ','.join(metaList[i])))

