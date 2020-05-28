import os

holder = dict()
ranges = []

def setDiscretization(value):
    fieldAdder("ho spatial discretization",value)
def setEigenvalueProblem(value):
    fieldAdder("do eigenvalue calculations",value)
def setFEPolynomialDegree(value):
    fieldAdder("finite element polynomial degree",value)
def setFirstThermalGroup(value):
    fieldAdder("thermal group boundary",value)
def setHaveReflectiveBC(value):
    fieldAdder("have reflective boundary",value)
def setNCells(value):
    fieldAdder("number of cells for x, y, z directions",value)
def setNEnergyGroups(value):
    fieldAdder("number of groups",value)
def setOutputFilenameBase(value):
    fieldAdder("output file name base",value)
def setReflectiveBoundary(value):
    fieldAdder("reflective boundary names",value)
def setSpatialDimension(value):
    fieldAdder("problem dimension",value)
def setSpatialMax(value):
    fieldAdder("x, y, z max values of boundary locations",value)
def setTransportModel(value):
    fieldAdder("transport model",value)

# Mesh
def setMeshGenerated(value):
    fieldAdder("is mesh generated by deal.II",value)
def setMeshFilename(value):
    fieldAdder("mesh file name",value)
def setUniformRefinements(value):
    fieldAdder("uniform refinements",value)
def setFuelPinRadius(value):
    fieldAdder("fuel Pin radius",value)
def setFuelPinTriangulation(value):
    fieldAdder("triangulation type of fuel Pin",value)
def setMeshPinResolved(value):
    fieldAdder("is mesh pin-resolved",value)

# Material Parameters
def setMaterialSubsection(value):
    fieldAdder("material ID map",value)
def setMaterialMapFilename(value):
    fieldAdder("material id file name",value)
def setMaterialFilenames(value):
    fieldAdder("material id file name map",value)
def setNumberOfMaterials(value):
    fieldAdder("number of materials",value)
def setFuelPinMaterialMapFilename(value):
    fieldAdder("fuel pin material id file name",value)

# Acceleration Parameters
def setPreconditioner(value):
    fieldAdder("ho preconditioner name",value)
def setBSSOR_Factor(value):
    fieldAdder("ho ssor factor",value)
def setDoNDA(value):
    fieldAdder("do nda",value)
def setNDA_Discretization(value):
    fieldAdder("nda spatial discretization",value)
def setNDALinearSolver(value):
    fieldAdder("nda linear solver name",value)
def setNDAPreconditioner(value):
    fieldAdder("nda preconditioner name",value)
def setNDA_BSSOR_Factor(value):
    fieldAdder("nda ssor factor",value)

# Solvers
def setEigenSolver(value):
    fieldAdder("eigen solver name",value)
def setInGroupSolver(value):
    fieldAdder("in group solver name",value)
def setLinearSolver(value):
    fieldAdder("ho linear solver name",value)
def setMultiGroupSolver(value):
    fieldAdder("mg solver name",value)

# Angular Quadrature
def setAngularQuad(value):
    fieldAdder("angular quadrature name",value)
def setAngularQuadOrder(value):
    fieldAdder("angular quadrature order",value)


def fieldAdder(field,value):
    global holder
    global ranges
    if type(value) is list: # you input a range of values for one 
        ranges.append((field, value))
        #print(ranges[0][1])
    else:
        holder[field] = value
    
def saveAs(filename):
    currDir = os.getcwd()
    try:
        os.mkdir("inputs")
    except FileExistsError:
        pass
    if len(ranges) > 0:
        print("huh")
        if len(ranges) > 1:
            print("Error—only try inputting one range right now")
        else:
            print(ranges[0][1])
            for i in range(len(ranges[0][1])):
                currFile = "inputs/" + filename+"-"+str(i)+".input"
                print(currFile)
                with open(currFile,"w") as file:
                    for j in holder:
                        file.write(j+ " = " + str(holder[j])+ "\n")


                    file.write(ranges[0][0] + " = " + str(ranges[0][1][i]) + "\n")
    else:
        with open("inputs/" + filename+".input","w") as file:
            for i in holder:
                file.write(i + " = " + str(holder[i]) + "\n")