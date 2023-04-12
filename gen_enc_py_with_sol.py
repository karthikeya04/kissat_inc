import sys
import re
import random

if (len(sys.argv) != 5):
    print("Incorrect invocation!!!\nPlease invoke as: ", sys.argv[0] + " num_mults known_sol_file prob_thresh out_file")
    quit()
    
random.seed(None)

nMult = int(sys.argv[1])
print(nMult, "\n")

given_sol = {}
sol_file_name = sys.argv[2]
if (sol_file_name != "none"):
    sol_file = open(sol_file_name, 'r')
    sol_contents = sol_file.readlines()
    for line in sol_contents:
        end_marker = re.match("EndOfSol", line)
        if end_marker:
            break
        A_match = re.match("(A\d\d\d+)", line)
        B_match = re.match("(B\d\d\d+)", line)
        G_match = re.match("(G\d\d\d+)", line)

        if (A_match != None):
            A_var = A_match.group(1)
            given_sol[A_var] = 1
        elif (B_match != None):
            B_var = B_match.group(1)
            given_sol[B_var] = 1
        elif (G_match != None):
            G_var = G_match.group(1)
            given_sol[G_var] = 1
        else:
            continue
    sol_file.close()

used_sol = {}
prob_thresh = float(sys.argv[3])
for i1 in range(1,4):
    for i2 in range(1,4):
        for l in range(1, nMult+1):
            var_name_A = f"A{i1}{i2}{l}"
            var_name_B = f"B{i1}{i2}{l}"
            var_name_G = f"G{i1}{i2}{l}"

            # With probability prob_thresh choose to retain the
            # value from the given solution for A vars.  Otherwise,
            # ignore the value from the given solution (denoted
            # by -1
            p = random.random()
            if ((sol_file_name != "none") and (p < prob_thresh)):
                if (var_name_A in given_sol):
                    used_sol[var_name_A] = 1
                else:
                    used_sol[var_name_A] = 0
            else:
                used_sol[var_name_A] = -1

            # Same thing as above for B vars
            p = random.random()
            if ((sol_file_name != "none") and (p < prob_thresh)):
                if (var_name_B in given_sol):
                    used_sol[var_name_B] = 1
                else:
                    used_sol[var_name_B] = 0
            else:
                used_sol[var_name_B] = -1
                
            #print(var_name_B + " ", used_sol[var_name_B], "\n")
                
            # Same thing as above for G vars
            p = random.random()
            if ((sol_file_name != "none") and (p < prob_thresh)):
                if (var_name_G in given_sol):
                    used_sol[var_name_G] = 1
                else:
                    used_sol[var_name_G] = 0
            else:
                used_sol[var_name_G] = -1

var2index = {}

# Generate the temporary variables and their indices for DIMACS file
index = 1
for i1 in range(1,4):
    for i2 in range(1,4):
        for j1 in range(1,4):
            for j2 in range(1,4):
                for l in range(1, nMult+1):
                    var_name = f"t{i1}{i2}{j1}{j2}{l}"
                    var2index[var_name] = index
                    index += 1

                    for k1 in range(1,4):
                        for k2 in range(1,4):
                            var_name = f"z{i1}{i2}{j1}{j2}{k1}{k2}{l}"
                            var2index[var_name] = index
                            index += 1

                            if ((l > 2) and ((l % 2) == 1)):
                                var_name = f"s{i1}{i2}{j1}{j2}{k1}{k2}_1_{l}"
                                var2index[var_name] = index
                                index += 1

#Now generate indices for A vars unless they are already set to values
#in used_sol
for i1 in range(1,4):
    for i2 in range(1,4):
        for l in range(1, nMult+1):
            var_name = f"A{i1}{i2}{l}"
            if (used_sol[var_name] != -1):
                continue
            var2index[var_name] = index
            index += 1

#Now generate indices for B vars unless they are already set to values
#in used_sol
for j1 in range(1,4):
    for j2 in range(1,4):
        for l in range(1, nMult+1):
            var_name = f"B{j1}{j2}{l}"
            if (used_sol[var_name] != -1):
                continue
            var2index[var_name] = index
            index += 1

#Now generate indices for G vars unless they are already set to values
#in used_sol
for j1 in range(1,4):
    for j2 in range(1,4):
        for l in range(1, nMult+1):
            var_name = f"G{j1}{j2}{l}"
            if (used_sol[var_name] != -1):
                continue
            var2index[var_name] = index
            index += 1

already_generated = {}

out_file_name = sys.argv[4]
temp_file = open("temp.dimacs", 'w')
clause_count = 0
var_count = index - 1

for i1 in range(1, 4):
    for i2 in range(1, 4):
        for j1 in range(1, 4):
            for j2 in range(1, 4):
                for ll in range(1, nMult+1):
                    A_entry = f"A{i1}{i2}{ll}"
                    B_entry = f"B{j1}{j2}{ll}"
                    ti1i2j1j2l = f"t{i1}{i2}{j1}{j2}{ll}"
                    u = var2index[ti1i2j1j2l]
                    if (used_sol[A_entry] == -1):
                        v = var2index[f"A{i1}{i2}{ll}"]
                    if (used_sol[B_entry] == -1):
                        w = var2index[f"B{j1}{j2}{ll}"]
                    
                    if (already_generated.get(ti1i2j1j2l) == None):
                        #Generate clauses for (ti1i2j1j2l <-> Ai1i2l.Bj1j2l)
                        comment = f"c (t{i1}{i2}{j1}{j2}{ll} <-> A{i1}{i2}{ll}.B{j1}{j2}{ll})"

                        
                        if ((used_sol[A_entry] == -1) and (used_sol[B_entry] == -1)):
                            clause1 = f"-{u} {v} 0"
                            clause2 = f"-{u} {w} 0"
                            clause3 = f"-{v} -{w} {u} 0"
                            clause_count += 3
                            temp_file.write(f"{comment}\n{clause1}\n{clause2}\n{clause3}\n\n")
                            
                        else:
                            if ((used_sol[A_entry] == 0) or (used_sol[B_entry] == 0)):
                                comment += f"\nc t{i1}{i2}{j1}{j2}{ll} <-> 0 since either A{i1}{i2}{ll} or B{j1}{j2}{ll} is 0"
                                clause1 = f"-{u} 0"
                                clause_count += 1
                                temp_file.write(f"{comment}\n{clause1}\n\n")
                            elif ((used_sol[A_entry] == 1) and (used_sol[B_entry] == 1)):
                                comment += f"\nc t{i1}{i2}{j1}{j2}{ll} <-> 1 since both A{i1}{i2}{ll} and B{j1}{j2}{ll} are 1"
                                clause1 = f"{u} 0"
                                clause_count += 1
                                temp_file.write(f"{comment}\n{clause1}\n\n")
                            elif (used_sol[A_entry] == 1):
                                comment += f"\nc t{i1}{i2}{j1}{j2}{ll} <-> B{j1}{j2}{ll} since A{i1}{i2}{ll} is 1"
                                clause1 = f"{u} -{w} 0"
                                clause2 = f"-{u} {w} 0"
                                clause_count += 2
                                temp_file.write(f"{comment}\n{clause1}\n{clause2}\n\n")
                            else: # (used_sol[B_entry] == 1):
                                comment += f"\nc t{i1}{i2}{j1}{j2}{ll} <-> A{j1}{j2}{ll} since B{i1}{i2}{ll} is 1"
                                clause1 = f"{u} -{v} 0"
                                clause2 = f"-{u} {v} 0"
                                clause_count += 2
                                temp_file.write(f"{comment}\n{clause1}\n{clause2}\n\n")
                                
                        already_generated[ti1i2j1j2l] = 1
                        

                for k1 in range(1, 4):
                    for k2 in range(1, 4):
                        for l in range(1, nMult+1):
                            G_entry = f"G{k1}{k2}{l}"

                            #Generate clauses for (zi1i2j1j2k1k2l <-> ti1i2j1j2l.Gk1k2l)
                            comment = f"c (z{i1}{i2}{j1}{j2}{k1}{k2}{l} <-> t{i1}{i2}{j1}{j2}{l}.G{k1}{k2}{l})"
                            u = var2index[f"z{i1}{i2}{j1}{j2}{k1}{k2}{l}"]
                            v = var2index[f"t{i1}{i2}{j1}{j2}{l}"]
                            if (used_sol[G_entry] == -1):
                                w = var2index[f"G{k1}{k2}{l}"]


                            if (used_sol[G_entry] == -1):
                                clause1 = f"-{u} {v} 0"
                                clause2 = f"-{u} {w} 0"
                                clause3 = f"-{v} -{w} {u} 0"
                                clause_count += 3
                                temp_file.write(f"{comment}\n{clause1}\n{clause2}\n{clause3}\n\n")
                            elif (used_sol[G_entry] == 0):
                                comment += f"\nc z{i1}{i2}{j1}{j2}{k1}{k2}{l} <-> 0 since G{k1}{k2}{l} is 0"
                                clause1 = f"-{u} 0"
                                clause_count += 1
                                temp_file.write(f"{comment}\n{clause1}\n\n")
                            else: # (used_sol[G_entry] == 1)
                                comment += f"\nc z{i1}{i2}{j1}{j2}{k1}{k2}{l} <-> t{i1}{i2}{j1}{j2}{l} since G{k1}{k2}{l} is 1"
                                clause1 = f"-{u} {v} 0"
                                clause2 = f"{u} -{v} 0"
                                clause_count += 2
                                temp_file.write(f"{comment}\n{clause1}\n{clause2}\n\n")
                                
                                
                            if ((l > 1) and (l % 2 == 1)):
                                if (l == 3):
                                    #Generate clauses for (si1i2j1j2k1k2_1_3 <-> zi1i2j1j2k1k21 xor zi1i2j1j2k1k22 xor zi1i2j1j2k1k23)
                                    comment = f"c (s{i1}{i2}{j1}{j2}{k1}{k2}_1_3 <-> z{i1}{i2}{j1}{j2}{k1}{k2}1 xor z{i1}{i2}{j1}{j2}{k1}{k2}2 xor z{i1}{i2}{j1}{j2}{k1}{k2}3)"
                                    u = var2index[f"s{i1}{i2}{j1}{j2}{k1}{k2}_1_3"]
                                    v = var2index[f"z{i1}{i2}{j1}{j2}{k1}{k2}1"]
                                    w = var2index[f"z{i1}{i2}{j1}{j2}{k2}{k2}2"]
                                    x = var2index[f"z{i1}{i2}{j1}{j2}{k1}{k2}3"]
                                    clause1 = f"-{u} {v} {w} {x} 0"
                                    clause2 = f"-{u} {v} -{w} -{x} 0"
                                    clause3 = f"-{u} -{v} -{w} {x} 0"
                                    clause4 = f"-{u} -{v} {w} -{x} 0"
                                    clause5 = f"{u} -{v} {w} {x} 0"
                                    clause6 = f"{u} -{v} -{w} -{x} 0"
                                    clause7 = f"{u} {v} -{w} {x} 0"
                                    clause8 = f"{u} {v} {w} -{x} 0"
                                    clause_count += 8
                                    temp_file.write(f"{comment}\n{clause1}\n{clause2}\n{clause3}\n{clause4}\n")
                                    temp_file.write(f"{clause5}\n{clause6}\n{clause7}\n{clause8}\n\n")

                                else:
                                    #Generate clauses for (si1i2j1j2k1k2_1_l <-> si1i2j1j2k1k2_1_{l-2} xor zi1i2j1j2k1k2{l-1} xor zi1i2j1j2k1k2{l})
                                    comment = f"c (s{i1}{i2}{j1}{j2}{k1}{k2}_1_{l} <-> s{i1}{i2}{j1}{j2}{k1}{k2}_1_{l-2} xor z{i1}{i2}{j1}{j2}{k1}{k2}{l-2} xor z{i1}{i2}{j1}{j2}{k1}{k2}{l})"
                                    u = var2index[f"s{i1}{i2}{j1}{j2}{k1}{k2}_1_{l}"]
                                    v = var2index[f"s{i1}{i2}{j1}{j2}{k1}{k2}_1_{l-2}"]
                                    w = var2index[f"z{i1}{i2}{j1}{j2}{k2}{k2}{l-1}"]
                                    x = var2index[f"z{i1}{i2}{j1}{j2}{k1}{k2}{l}"]
                                    clause1 = f"-{u} {v} {w} {x} 0"
                                    clause2 = f"-{u} {v} -{w} -{x} 0"
                                    clause3 = f"-{u} -{v} -{w} {x} 0"
                                    clause4 = f"-{u} -{v} {w} -{x} 0"
                                    clause5 = f"{u} -{v} {w} {x} 0"
                                    clause6 = f"{u} -{v} -{w} -{x} 0"
                                    clause7 = f"{u} {v} -{w} {x} 0"
                                    clause8 = f"{u} {v} {w} -{x} 0"
                                    clause_count += 8
                                    temp_file.write(f"{comment}\n{clause1}\n{clause2}\n{clause3}\n{clause4}\n")
                                    temp_file.write(f"{clause5}\n{clause6}\n{clause7}\n{clause8}\n\n")
                        # end of for l in range(1,nMult+1)

                        #Generate clause si1i2j1j2k1k2_1_23 if RHS of Brent equation for i1i2j1j2k1k2 = 1
                        #Otherwise, generate clause ~si1i2j1j2k1k2_1_23
                        si1i2j1j2k1k2_1_nMult = f"s{i1}{i2}{j1}{j2}{k1}{k2}_1_{nMult}"
                        temp = var2index[si1i2j1j2k1k2_1_nMult]
                        if ((i2 == j1) and (i1 == k1) and (j2 == k2)):
                            comment = f"c RHS of Brent eqn for i1={i1}, i2={i2}, j1={j1}, j2={j2}, k1={k1}, k2={k2} is 1, hence {si1i2j1j2k1k2_1_nMult}"
                            clause1 = f"{temp} 0"
                        else:
                            comment = f"c RHS of Brent eqn for i1={i1}, i2={i2}, j1={j1}, j2={j2}, k1={k1}, k2={k2} is 0, hence ~{si1i2j1j2k1k2_1_nMult}"
                            clause1 = f"-{temp} 0"
                        
                        clause_count += 1
                        temp_file.write(f"{comment}\n{clause1}\n\n")

temp_file.close()

out_file = open(out_file_name, 'w')
temp_file = open("temp.dimacs", 'r')

out_file.write(f"p cnf {var_count} {clause_count}\n\n")
for line in temp_file:
    out_file.write(line)

temp_file.close()
out_file.close()

                            
