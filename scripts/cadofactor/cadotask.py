#!/usr/bin/env python3
import sqlite3
from datetime import datetime
import re
import os.path
import wudb
import patterns
import cadoprograms
import cadologger

# TODO:
# Job status. Should probably be in the DB - no need to have another file sitting around with another file format to parse
# Check which tasks need to be done:
#   1. Parameter generation
#   2. Polynomial selection (add WUs to DB until enough polynomials checked/good enough found)
#   3. Factorbase (if hosted on server)
#   4. Free relations
#   5. Sieving until enough relations received (generating WUs on the fly, adding to DB, distributing to clients, checking results)
#   6. Remove duplicates
#   7. Remove singletons, check, go back to Sieving if necessary (simply increase rels_wanted)
#   8. Merge
#   9. Call BWC
#   10. SQRT
#   11. Check complete factorization

# Some parameters are provided by the param file but can change during
# the factorization, like rels_wanted. On one hand, we want automatic 
# updates to parameters to be stored in the DB, otoh, we want to allow
# externally setting new parameters. Need to distinguish between new 
# external parameters that overwrite DB, and old external parameters 
# that don't overwrite. Or maybe two ways to specify external params:
# --defaults which does not overwrite, and --forceparam which does

class Task(patterns.Observable, patterns.Observer):
    """ A base class that represents one task that needs to be processed. 
    
    Sub-classes must define class variables:
        name: the name of the task in a simple form that can be used as
            a Python dictionary key, a directory name, part of a file name,
            part of an SQL table name, etc. That pretty much limits it to
            alphabetic first letter, and alphanumeric rest. 
        title: A pretty name for the task, will be used in screen output
        programs: A list of classes of Programs which this tasks uses
        paramnames: A list of parameter keywords which this task uses.
            This is used for extracting relevant parameters from the parameter
            hierarchical dictionary.
    """
    # Parameters that all tasks use
    paramnames = ("name", "workdir")

    def __init__(self, dependencies, *args, db = None, parameters = None, \
                **kwargs):
        ''' Sets up a database connection and a DB-backed dictionary for 
        parameters. Reads parameters from DB, and merges with hierarchical
        parameters in the parameters argument. Parameters passed in by 
        parameters argument do not override values in the DB-backed 
        parameter dictionary.
        '''

        super().__init__(*args, **kwargs)
        if dependencies:
            self.dependencies = dependencies
            for d in dependencies:
                d.subscribeObserver(self)
        else:
            self.dependencies = None
        self.logger = cadologger.Logger()
        self.logger.debug("Enter Task.__init__(%s)", 
                          self.name)
        if False:
            self.logger.debug("Enter Task.__init__(): parameters = %s", 
                              parameters)
        # DB-backed dictionary with parameters for this task
        self.db = db
        self.params = wudb.DictDbAccess(self.db, self.tablename())
        # Derived class must define name
        # Set default parametes for this task, if any are given
        if parameters:
            # Add the common paramnames defined in the Task class to the 
            # (presumably class-defined) paramnames in self, and bind the 
            # result to an instance variable
            self.paramnames = Task.paramnames + self.paramnames
            defaults = parameters.myparams(self.paramnames, self.parampath)
            self.params.setdefault(None, defaults)
        self.logger.debug("%s: params = %s", self.title, self.params)
        # Set default parameters for our programs
        self.progparams = []
        for prog in self.programs:
            progparams = wudb.DictDbAccess(self.db, self.prog_tablename(prog))
            self.progparams.append(progparams)
            if parameters:
                defaults = parameters.myparams(
                    prog.params_dict(), [self.parampath, prog.name])
                progparams.setdefault(None, defaults)
        self.logger.debug("Exit Task.__init__(%s)", self.name)
        return
    
    @staticmethod
    def check_tablename(name):
        no_ = name.replace("_", "")
        if not no_[0].isalpha() or not no_[1:].isalnum():
            raise Exception("%s is not valid for an SQL table name" % name)

    def tablename(self, extra = None):
        """ Return the table name for the DB-backed dictionary with parameters
        for the current task """
        # Maybe replace SQL-disallowed characters here, like digits and '.' ? 
        # Could be tricky to avoid collisions
        self.check_tablename(self.name)
        if extra:
            self.check_tablename(extra)
            return self.name + '_' + extra
        else:
            return self.name
    
    def prog_tablename(self, prog):
        """ Return the table name for the DB-backed dictionary with parameters 
        for program prog
        """
        return self.tablename(prog.name)
    
    def is_done(self):
        return False
    
    def run(self, parameters = None):
        ''' Runs the prerequisites. Sub-classes should call this first in 
        their run() method.
        
        Parameters passed in by parameters DO override values in the
        DB-backed parameter dictionary. 
        '''
        self.logger.debug("Enter Task.run(%s), parameters = %s", 
                          self.name, parameters)
        self.logger.debug("Task.run(%s): self.is_done() = %s", 
                          self.name, self.is_done())
        # Check/run the prerequisites
        if not self.dependencies is None:
            for task in self.dependencies:
                if not task.is_done():
                    self.logger.debug("Task.run(%s): Running prerequisite %s",
                                      self.name, task.name)
                    task.run(parameters)
        
        # Set parameters for our task and programs
        if parameters:
            # Override this task's parameters, if parameters are given
            update = parameters.myparams(self.paramnames, self.parampath)
            self.params.update(None, update)
            # Override the programs' parameters, if parameters are given
            for (index, prog) in enumerate(self.programs):
                update = parameters.myparams(
                    prog.params_dict(), [self.parampath, prog.name])
                self.progparams[index].update(None, update)
        
        self.logger.debug("Exit Task.run(" + self.name + ")")
        return
    
    def make_output_filename(self, name):
        """ Make a filename of the form workdir/jobname.taskname.name """
        return "%s%s%s.%s.%s" % (
                self.params["workdir"], os.sep, self.params["name"], 
                self.name, name)
    
    def make_output_dirname(self, extra = None):
        """ Make a directory name of the form workdir/jobname.taskname/ if 
        extra is not given, or workdir/jobname.taskname/extra/ if it is
        """
        if extra:
            return "%s%s%s.%s%s%s%s" % (
                self.params["workdir"], os.sep, self.params["name"], 
                self.name, os.sep, extra, os.sep)
        else:
            return "%s%s%s.%s%s" % (
                self.params["workdir"], os.sep, self.params["name"], 
                self.name, os.sep)
    
    def check_input_files(self, filenames):
        """ Check that the files in "filenames" exist.
        
        Raises IOError if any files do not exists, return None
        """
        for filename in filenames:
            if not os.path.isfile(filename):
                raise IOError("%s input file %s does not exist" 
                              % (self.title, filename))
        return
    
    def check_output_files(self, filenames, shouldexist):
        """ Check that the output files in "filenames" exist or don't exist, 
        according to shouldexist.
        
        Raise IOError if any check fails, return None
        """
        for f in filenames:
            exists = os.path.isfile(f)
            if shouldexist and not exists:
                raise IOError("%s output file %s does not exist" % 
                                (self.title, f))
            elif not shouldexist and exists:
                raise IOError("%s output file %s already exists" % 
                                (self.title, f, filename))
        return
    
    def submit(self, commands, inputfiles, outputfiles, tempfiles):
        ''' Submit a command that needs to be run. Returns a handle
        which can be used for status check.

        The inputfiles parameters is a list of input files that program 
        needs; they are not automatically filled into the command line(s),
        but need to be listed on the command line(s) explicitly. The are 
        input files list is used to generate FILE lines in work units, 
        for example.
        
        The outputfiles parameter lists the output files that running the 
        command will produce. They are used to produce RESULT lines in 
        workunits. Note that the final file names of the output files may 
        differ, for example, when the output files are uploaded to the server
        and stored under a unique file name.

        The tempfiles parameter lists temporary files that should be deleted 
        after the commands ran successfully. There are used to generate DELETE 
        lines in work units.
        '''
        
        # We should actually generate the workunits here and feed them
        # to the workunit processor. Avoids duplicate code and tests 
        # more of the code path. Status of the workunits should be kept
        # in a wudb, just like the server would. I.e., we do everything
        # here as in the client/server setup, except we skip the server
        # and handing WUs to the wu processor directly
        pass
    
    def status(self, handle):
        ''' Check status of previously submitted commands 
        
        Returns the execution status and the list of output files produced
        '''
        return (status, outputfiles)

    def updateObserver(self, message):
        pass


class ClientServerTask(Task):
    def __init__(self, server = None, *args, **kwargs):
        self.server = server
        super().__init__(*args, **kwargs)

    def submit(self, commands, inputfiles, outputfiles, tempfiles):
        ''' Submit a command that needs to be run. Uses client/server.
        Returns an index which can be used for status check
        '''
        pass
    
    def status(self, index):
        ''' Check status of previously submitted commands '''
        pass


# Each task has positional parameters with references to the tasks from 
# which it receives its inputs. This will be used to query the referenced 
# tasks' parameters, list of output files, etc. - all the info the current 
# task needs to run and which is produced by previous tasks. Since the 
# dependencies are an intrinsic property of a given task, they are passed 
# as positional parameters rather than as, say, a list. The abstract 
# superclass Task receives the dependecies as a list; this list is stored 
# and used for graph traversal which is implemented in Task.

class Polynomial(object):
    # Keys that can occur in a polynomial file in their preferred ordering,
    # and whether the key is mandatory or not. The preferred ordering is used
    # when turning a polynomial back into a string. 
    paramnames = ("rlim", "alim", "lpbr", "lpba", "mfbr", "mfba", "rlambda", 
                  "alambda")
    keys = ( ("n", True), ("Y0", True), ("Y1", True), ("c0", True), 
             ("c1", True), ("c2", True), ("c3", True), ("c4", True),
             ("c5", False), ("c6", False), ("m", True), ("skew", True) )
    
    def __init__(self, lines):
        """ Parse a polynomial file in the syntax as produced by polyselect2l 
        """
        self.poly = None
        self.E = 0.
        poly = {}
        for line in lines:
            # print ("Parsing line: >%s<" % line)
            # If there is a "No polynomial found" message anywhere in the
            # file, we assume that there is no polynomial. This assumption
            # will be false if, e.g., files get concatenated
            if re.match("No polynomial found", line):
                return
            # If this is a comment line telling the Murphy E value, 
            # extract the value and store it
            match = re.match("\s*#\s*MurphyE\s*\(.*\)=(.*)$", line)
            if match:
                self.E = float(match.group(1))
                continue
            # Drop comment, strip whitespace
            l = line.split('#', 1)[0].strip()
            # If nothing is left, process next line
            if not l:
                continue
            # All remaining lines must be of the form "x: y"
            a = l.split(":")
            if not len(a) == 2:
                raise Exception("Invalid line %s" % l)
            key = a[0].strip()
            value = a[1].strip()
            if not key in dict(self.keys):
                raise Exception("Invalid key %s in line %s" %
                                (key, l))
            poly[key] = value
        for (key, isrequired) in self.keys:
            if isrequired and not key in poly:
                raise Exception("Key %s missing" % key)
        self.poly = poly
        return
    
    def __str__(self):
        arr = [(key + ": " + self.poly[key] + '\n')
               for (key,req) in self.keys if key in self.poly]
        return "".join(arr)

    def __eq__(self, other):
        return self.poly == other.poly
    def __ne__(self, other):
        return self.poly != other.poly

    def is_valid(self):
        return not self.poly is None
    
    def setE(self, E):
        self.E = float(E)
    
    def create_file(self, filename, params):
        # Write polynomial to a file, and add lines with parameters such as 
        # "alim" if supplied in params 
        with open(filename, "w") as f:
            f.write(str(self))
            for key in self.paramnames:
                if key in params:
                    f.write(key + ": %s\n" % params[key])


class PolyselTask(Task):
    """ Finds a polynomial, uses client/server """
    name = "polyselect"
    parampath = "tasks." + name
    title = "Polynomial Selection"
    programs = (cadoprograms.Polyselect2l,)
    paramnames = ("adrange", "P", "N", "admin", "admax")
    
    def __init__(self, *args, **kwargs):
        super().__init__(*args, dependencies = None, **kwargs)
        self.params.setdefault("admin", "0")
        self.params.setdefault("adnext", "0")
        if int(self.params["adnext"]) < int(self.params["admin"]):
            self.params["adnext"] = self.params["admin"]
    
    def is_done(self):
        # self.logger.debug ("PolyselTask.is_done(): Task parameters: %s", 
        #                    self.params)
        return int(self.params["adnext"]) >= int(self.params["admax"])
    
    def run(self, parameters = None):
        # Make command line for polselect2l, run it. 
        # Whole range in one go for now
        self.logger.debug("Enter PolyselTask.run(" + self.name + ")")
        super().run(parameters = parameters)
        
        self.logger.info("Beginning %s", self.title)
        self.logger.debug("PolyselTask.run(): Task parameters: %s", 
                          self.params)
        
        if "bestpoly" in self.params:
            bestpoly = Polynomial(self.params["bestpoly"].splitlines())
            bestpoly.setE(self.params["bestE"])
            self.logger.info("Best polynomial previously found in %s has "
                             "Murphy_E = %g", 
                             self.params["bestfile"], bestpoly.E)
            self.notifyObservers({"poly": bestpoly})
        else:
            bestpoly = None
            self.logger.info("No polynomial was previously found")
        
        while not self.is_done():
            adstart = int(self.params["adnext"])
            adend = adstart + int(self.params["adrange"])
            polyselect_params = self.progparams[0].copy()
            polyselect_params["admin"] = str(adstart)
            polyselect_params["admax"] = str(adend)
            outputfile = self.make_output_filename("%d-%d" % (adstart, adend))
            if os.path.isfile(outputfile):
                self.logger.info("%s already exists, won't generate again",
                                 outputfile)
            else:
                try:
                    p = self.programs[0](stdout = outputfile, 
                                         kwargs = polyselect_params)
                    p.run()
                    p.wait()
                except Exception as e:
                    self.logger.error("Error running %s: %s", 
                                      self.programs[0].name, e)
                    outputfile = None
            
            self.params["adnext"] = str(adend)
            
            poly = None
            if outputfile:
                with open(outputfile, "r") as f:
                    try:
                        poly = Polynomial(f)
                    except Exception as e:
                        self.logger.error("Invalid polyselect file %s: %s", 
                                          outputfile, e)
            
            if not poly or not poly.is_valid():
                self.logger.info('No polynomial found in %s', outputfile)
            elif not poly.E:
                self.logger.error("Polynomial in file %s has no Murphy E value" 
                                  % outputfile)
            elif not bestpoly or poly.E > bestpoly.E:
                bestpoly = poly
                self.params["bestE"] = str(poly.E)
                self.params["bestpoly"] = str(poly)
                self.params["bestfile"] = outputfile
                self.logger.info("New best polynomial from file %s:"
                                 " Murphy E = %g" % (outputfile, poly.E))
                self.logger.debug("%s", poly)
                self.notifyObservers({self.name: bestpoly})
            else:
                self.logger.info("Best polynomial from file %s with E=%g is "
                                 "no better than current best with E=%g",
                                 outputfile, poly.E, bestpoly.E)
            # print(poly)
        
        self.logger.info("%s finished", self.title)
        if not bestpoly:
            self.logger.error ("No polynomial found")
            return
        self.logger.info("Best polynomial from %s has Murphy_E =  %g", 
                          self.params["bestfile"] , bestpoly.E)
        self.logger.debug("Exit PolyselTask.run(" + self.name + ")")
        return
    
    def get_poly(self):
        if "bestpoly" in self.params:
            return Polynomial(self.params["bestpoly"].splitlines())
        else:
            return None


class FactorBaseOrFreerelTask(Task):
    """ Common base class for programs that produce one output file from 
    the polynomial, i.e., factorbase and freerel 
    """
    
    def __init__(self, polyselect, *args, **kwargs):
        super().__init__(*args, dependencies = (polyselect,), **kwargs)
        self.polyselect = polyselect
        # Invariant: if we have a result (in self.params[targetkey]) then we
        # must also have a polynomial (in self.params["poly"])
        self.targetkey = self.target + "file"
        if self.targetkey in self.params:
            assert "poly" in self.params
            # The target file must correspond to the polynomial "poly"
    
    def run(self, parameters = None):
        self.logger.debug("Enter FactorBaseOrFreerelTask.run(%s)", self.name)
        super().run(parameters = parameters)
        
        # Get best polynomial found by polyselect
        poly = self.polyselect.get_poly()
        if not poly:
            raise Exception("FactorBaseOrFreerelTask(): no polynomial received")
        
        # Check if we have already computed the target file for this polynomial
        if "poly" in self.params:
            prevpoly = Polynomial(self.params["poly"].splitlines())
            if poly != prevpoly:
                if self.targetkey in self.params:
                    del(self.params[self.targetkey])
                self.params["poly"] = str(poly)
        else:
            self.params["poly"] = str(poly)
        
        if not self.is_done():
            # Write polynomial to a file
            polyfile = self.make_output_filename("poly")
            poly.create_file(polyfile, self.params)
            
            # Make file name for factor base file
            outputfile = self.make_output_filename(self.target)
            args = ()
            kwargs = self.progparams[0].copy()
            kwargs["poly"] = polyfile
            if "pmin" in self.programs[0].params_dict():
                kwargs.setdefault("pmin", "1")
            if "pmax" in self.programs[0].params_dict():
                kwargs.setdefault("pmax", str(2**int(self.params["lpba"])))
            p = self.programs[0](kwargs = kwargs, stdout = outputfile)
            p.run()
            p.wait()
            
            self.params[self.targetkey] = os.path.realpath(outputfile)
            self.notifyObservers({self.name: outputfile})
        self.logger.debug("Exit FactorBaseOrFreerelTask.run(%s)", self.name)
    
    def is_done(self):
        if self.targetkey in self.params \
                and not os.path.isfile(self.params[self.targetkey]):
            raise Exception("FactorBaseOrFreerelTask.is_done(%s): marked "
                            "as done but target file %s does not exist" % 
                            (self.name, self.params[self.targetkey]))
        return self.targetkey in self.params
    
    def updateObserver(self, message):
        if isinstance(message, Polynomial):
            # We might start factorbase computation while polynomial 
            # selection is still running
            pass
    
    def get_filename(self):
        if self.targetkey in self.params:
            return self.params[self.targetkey]
        else:
            return None

class FactorBaseTask(FactorBaseOrFreerelTask):
    """ Generates the factor base for the polynomial(s) """
    name = "factorbase"
    title = "Generate Factor Base"
    programs = (cadoprograms.MakeFB,)
    parampath = "tasks." + name
    paramnames = ("alim", )
    target = "roots"


class FreeRelTask(FactorBaseOrFreerelTask):
    """ Generates free relations for the polynomial(s) """
    name = "freerel"
    title = "Generate Free Relations"
    programs = (cadoprograms.FreeRel,)
    parampath = "tasks." + name
    paramnames = ("lpba", )
    target = "freerel"


class SievingTask(Task):
    """ Does the sieving, uses client/server """
    name = "sieving"
    title = "Lattice Sieving"
    programs = (cadoprograms.Las,)
    parampath = "tasks." + name
    paramnames = ("qmin", "qrange", "rels_wanted") + Polynomial.paramnames
    
    def __init__(self, polyselect, factorbase, *args, **kwargs):
        super().__init__(*args, dependencies = (polyselect, factorbase), **kwargs)
        self.polyselect = polyselect
        self.factorbase = factorbase
        self.params.setdefault("qmin", self.params["alim"])
        self.params.setdefault("qnext", self.params["qmin"])
        self.params.setdefault("rels_found", "0")
        self.files = wudb.DictDbAccess(self.db, self.tablename("files"))
    
    def run(self, parameters = None):
        self.logger.debug("Enter SievingTask.run(" + self.name + ")")
        super().run(parameters = parameters)

        # Get best polynomial found by polyselect
        poly = self.polyselect.get_poly()
        if not poly:
            raise Exception("SievingTask(): no polynomial received")
        # Write polynomial to a file
        polyfile = self.make_output_filename("poly")
        poly.create_file(polyfile, self.params)
        
        while not self.is_done():
            args = ()
            kwargs = self.progparams[0].copy()
            q0 = int(self.params["qnext"])
            q1 = q0 + int(self.params["qrange"])
            outputfile = self.make_output_filename("%d-%d" % (q0, q1))
            kwargs["q0"] = str(q0)
            kwargs["q1"] = str(q1)
            kwargs["poly"] = polyfile
            kwargs["factorbase"] = \
                self.factorbase.get_filename()
            kwargs["out"] = outputfile
            p = self.programs[0](args, kwargs)
            p.run()
            p.wait()
            self.params["qnext"] = str(q1)
            rels = self.check_relfile(outputfile)
            if rels == None:
                raise Exception("Siever output file %s invalid" % outputfile)
            total = int(self.params["rels_found"]) + rels
            self.params["rels_found"] = str(total)
            self.files[outputfile] = str(rels)
            self.logger.info("Found %d relations in %s, total is now %d", 
                             rels, outputfile, total)
            self.notifyObservers({self.name, outputfile})
        self.logger.debug("Exit SievingTask.run(" + self.name + ")")
        return
    
    @staticmethod
    def check_relfile(filename):
        size = os.path.getsize(filename)
        with open(filename, "r") as f:
            f.seek(max(size - 1000, 0))
            for line in f:
                match = re.match("# Total (\d+) reports ", line)
                if match:
                    return int(match.group(1))
        return None
    
    def get_filenames(self):
        return self.files.keys()
    
    def is_done(self):
        return int(self.params["rels_found"])>=int(self.params["rels_wanted"]) 


class Duplicates1Task(Task):
    """ Removes duplicate relations """
    name = "duplicates1"
    title = "Filtering: Duplicate Removal, splitting pass"
    programs = (cadoprograms.Duplicates1,)
    parampath = "tasks.filtering." + name
    paramnames = ("nslices_log",)
    
    def __init__(self, sieving, *args, **kwargs):
        super().__init__(*args, dependencies = (sieving,), **kwargs)
        self.sieving = sieving
        self.parts = 2**int(self.params["nslices_log"])
        self.already_split_input = \
            wudb.DictDbAccess(self.db, self.tablename("infiles"))
        self.already_split_output = \
            wudb.DictDbAccess(self.db, self.tablename("outfiles"))
    
    def run(self, parameters = None):
        self.logger.debug("Enter Duplicates1Task.run(" + self.name + ")")
        super().run(parameters = parameters)
        # Check that previously split files were split into the same number
        # of pieces that we want now
        for (infile, parts) in self.already_split_input.items():
            if int(parts) != self.parts:
                # TODO: ask interactively (or by -recover) whether to delete 
                # old output files and generate new ones, if input file is 
                # still available
                raise Exception("%s was previously split into %d parts, "
                                "now %d parts requested",
                                infile, int(parts), self.parts)
            for outfile in self.make_output_filenames(infile):
                if not outfile in self.already_split_output:
                    # TODO: How to recover from this error? Just re-split again?
                    raise Exception("Output file %s missing in database for "
                                    "supposedly split input file %s" %
                                    (outfile, infile))
            
        
        # Check that previously split files do, in fact, exist.
        # FIXME: Do we want this? It may be slow when there are many files.
        # Reading the directory and comparing the lists would probably be
        # faster than individual lookups.
        self.check_output_files(self.get_filenames(), shouldexist=True)
        
        files = self.sieving.get_filenames()
        newfiles = [f for f in files if not f in self.already_split_input]
        self.logger.debug ("%s: new files to split are: %s", 
                           self.title, newfiles)
        
        if newfiles:
            self.make_directories()
            # TODO: can we recover from missing input files? Ask Sieving to
            # generate them again? Just ignore the missing ones?
            self.check_input_files(newfiles)
            # Split the new files
            for f in newfiles:
                if self.parts == 1:
                    # If we should split into only 1 part, we don't actually
                    # split at all. We simply write the input file name
                    # to the table of output files, so the next stages will 
                    # read the original siever output file, thus avoiding 
                    # having another copy of the data on disk
                    outfilenames = {f:"0"}
                else:
                    outfilenames = self.make_output_filenames(f)
                    # TODO: how to recover from existing output files?
                    # Simply re-split? Check whether they all exist and assume 
                    # they are correct if they do?
                    self.check_output_files(outfilenames.keys(),
                                            shouldexist=False)
                    kwargs = self.progparams[0].copy()
                    kwargs["out"] = self.make_output_dirname()
                    p = self.programs[0]((f,), kwargs)
                    p.run()
                    p.wait()
                    # Check that the output files exist now
                    # TODO: How to recover from error? Presumably a dup1
                    # process failed, but that should raise a return code
                    # exception
                    self.check_output_files(outfilenames.keys(),
                                            shouldexist=True)
                self.already_split_input[f] = str(self.parts)
                self.already_split_output.update(outfilenames)
        self.logger.debug("Exit Duplicates1Task.run(" + self.name + ")")
        return
    
    def make_output_filename(self, name, I):
        """ Make the output file names corresponding to slice I of the input
        file named "name"
        """
        basefile = os.path.basename(name)
        basedir = self.make_output_dirname(str(I))
        return basedir + basefile
    
    def make_output_filenames(self, name):
        """ Make a dictioary of the output file names corresponding to the
        input file named "name" as keys, and the slice number as a string
        as value
        """
        return {self.make_output_filename(name, I):str(I) \
                for I in range(0, self.parts)}
    
    def make_directories(self):
        basedir = self.make_output_dirname()
        if not os.path.isdir(basedir):
            os.mkdir(basedir)
        for I in range(0, self.parts):
            dirname = self.make_output_dirname(str(I))
            if not os.path.isdir(dirname):
                os.mkdir(dirname)
        return
    
    def get_filenames(self, slice_nr = None):
        """ Return an array of filenames of the already split output files
        
        If slice_nr is given, return only files for that slice
        """
        return [f for (f,s) in self.already_split_output.items() \
                    if slice_nr is None or int(s) == slice_nr]

class Duplicates2Task(Task):
    """ Removes duplicate relations """
    name = "duplicates2"
    title = "Filtering: Duplicate Removal, removal pass"
    programs = (cadoprograms.Duplicates2,)
    parampath = "tasks.filtering." + name
    paramnames = ()
    
    def __init__(self, duplicates1, *args, **kwargs):
        super().__init__(*args, dependencies = (duplicates1,), **kwargs)
        self.duplicates1 = duplicates1
    
    def run(self, parameters = None):
        self.logger.debug("Enter Duplicates2Task.run(" + self.name + ")")
        super().run(parameters = parameters)
        files = self.duplicates1.get_filenames()
        kwargs = self.progparams[0].copy()
        p = self.programs[0](None, kwargs)
        p.run()
        p.wait()
        
        self.logger.debug("Exit Duplicates2Task.run(" + self.name + ")")

    def split_files(self, filelist):
        pass

    def remove_duplicates(self, filelist):
        pass

class PurgeTask(Task):
    """ Removes singletons and computes excess """
    name = "singletons"
    title = "Filtering: Singleton removal"
    programs = (cadoprograms.Purge,)
    parampath = "tasks.filtering." + name
    paramnames = ()
    
    def __init__(self, duplicates2, *args, **kwargs):
        super().__init__(*args, dependencies = (duplicates2,), **kwargs)
        self.duplicates2 = duplicates2

    def run(self, parameters = None):
        self.logger.debug("Enter PurgeTask.run(" + self.name + ")")
        super().run(parameters = parameters)
        if not self.is_done():
            args = ()
            kwargs = self.progparams[0].copy()
            p = self.programs[0](args, kwargs)
            p.run()
            p.wait()
        self.logger.debug("Exit PurgeTask.run(" + self.name + ")")


class MergeTask(Task):
    """ Merges relations """
    name = "merging"
    title = "Filtering: Merging"
    programs = (cadoprograms.Merge,)
    parampath = "tasks.filtering." + name
    paramnames = ()
    
    def __init__(self, freerel, unique, *args, **kwargs):
        super().__init__(*args, dependencies = (freerel, unique), **kwargs)

    def run(self, parameters = None):
        self.logger.debug("Enter MergeTask.run(" + self.name + ")")
        super().run(parameters = parameters)
        if not self.is_done():
            args = ()
            kwargs = self.progparams[0].copy()
            p = self.programs[0](args, kwargs)
            p.run()
            p.wait()
        self.logger.debug("Exit MergeTask.run(" + self.name + ")")


class LinAlgTask(Task):
    """ Runs the linear algebra step """
    name = "linalg"
    title = "Linear Algebra"
    programs = (cadoprograms.BWC,)
    parampath = "tasks." + name
    paramnames = ()
    
    def __init__(self, merge, *args, **kwargs):
        super().__init__(*args, dependencies = (merge,), **kwargs)

    def run(self, parameters = None):
        self.logger.debug("Enter LinAlgTask.run(" + self.name + ")")
        super().run(parameters = parameters)
        if not self.is_done():
            args = ()
            kwargs = self.progparams[0].copy()
            p = self.programs[0](args, kwargs)
            p.run()
            p.wait()
        self.logger.debug("Exit LinAlgTask.run(" + self.name + ")")


class SqrtTask(Task):
    """ Runs the square root """
    name = "sqrt"
    title = "Square Root"
    programs = (cadoprograms.Sqrt,)
    parampath = "tasks." + name
    paramnames = ()
    
    def __init__(self, polyselect, freerel, sieving, merge, linalg, 
                 *args, **kwargs):
        dep = (polyselect, freerel, sieving, merge)
        super().__init__(*args, dependencies = dep, **kwargs)

    def run(self, parameters = None):
        self.logger.debug("Enter SqrtTask.run(" + self.name + ")")
        super().run(parameters = parameters)
        if not self.is_done():
            args = ()
            kwargs = self.progparams[0].copy()
            p = self.programs[0](args, kwargs)
            p.run()
            p.wait()
        self.logger.debug("Exit SqrtTask.run(" + self.name + ")")


