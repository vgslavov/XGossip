/*
 * $Id: example.cpp,v 1.3 2001/09/03 22:11:55 sbooth Exp $ 
 *
 * Skeleton of a CGI application written using the GNU cgicc library
 */
/*
 *  Praveen Rao, UMKC, 2008.
 */

#include <exception>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/time.h>

#include "cgicc/Cgicc.h"
#include "cgicc/HTTPHeaders.h"
#include "cgicc/HTMLClasses.h"
#include "defs.h"
#include "utils.h"

// To use the debug logging feature, the variable gLogFile MUST be
// defined, and it _must_ be an ofstream
#if DEBUG
  ofstream gLogFile( "./cgicc.log", ios::app );
#endif

#if CGICC_USE_NAMESPACES
  using namespace std;
  using namespace cgicc;
#else
#  define div div_
#  define select select_
#endif

int
main(int argc, 
     char **argv)
{
  try {
    Cgicc cgi;
    
    // Output the HTTP headers for an HTML document, and the HTML 4.0 DTD info
    cout << HTTPHTMLHeader() << HTMLDoctype(HTMLDoctype::eStrict) << endl;
    //cout << html().set("lang", "en").set("dir", "ltr") << endl;

    // Set up the page's header and title.
    cout << head() << endl;
    cout << title() << "psiX" << cgi.getVersion() << title() << endl;
    cout << head() << endl;
    
    // Start the HTML body
    cout << body() << endl;

    // Print out a message
    cout << h2("Publishing under progress...");
    cout << h3("<font color=brown>Note that remote copy and SSH to some PlanetLab nodes can take more than a few seconds. Please be patient...</font>") << endl;
    // Content
    const CgiEnvironment& env = cgi.getEnvironment();
    
    char cmd[1024];
    double currTime = getgtod();
    char myTime[1024];
    sprintf(myTime, "%lf", currTime);

    string XMLFILE = WORKINGDIR + env.getRemoteAddr() + string(myTime) + ".xml";
    string XMLDTD = WORKINGDIR + env.getRemoteAddr() + string(myTime) + ".dtd";
    sprintf(cmd, "rm -rf %s/*%s*", WORKINGDIR, env.getRemoteAddr().c_str());
    //cout << cmd;
    //system(cmd);
    char docid[128] = "";
    string postData = env.getPostData();

    char *ptr = strstr(postData.c_str(), "\"docid\"");
    if (ptr != NULL) {
      sscanf(ptr+strlen("\"docid\"")+strlen("\r\n\r\n"), "%[^\r\n]", docid);
    }
    // If empty actually
    if (strstr(docid, "--------------")) {
      docid[0] = 0;
    }

    if (strlen(docid) == 0) {
      cout << "Docid is EMPTY" << endl;
      cout << body() << html() << endl;
      return 0;
    }

    const_file_iterator file;
    file = cgi.getFile("xmlfile");
    if (file != cgi.getFiles().end()) { 
      ofstream xmlfile(XMLFILE.c_str());
      (*file).writeToStream(xmlfile);
      xmlfile.close();
    }

    file = cgi.getFile("xmldtd");
    if (file != cgi.getFiles().end()) {
      ofstream xmldtd(XMLDTD.c_str());
      (*file).writeToStream(xmldtd);
      xmldtd.close();
    }

    // Create a signature
    setenv("LD_LIBRARY_PATH", "./lib", 1);
     
    struct stat e;
    // Check if DTD is actually passed
    if (stat(XMLDTD.c_str(), &e) == -1) {
      //cout << body() << br() << "Creating signatures without DTD" << endl;
      sprintf(cmd, "./XMLSignGenerator %s junk 0 %d /tmp/sig.%s.%lf 5 %s >& /tmp/%s.%lf.errorfile", XMLFILE.c_str(), IRRPOLYVAL, env.getRemoteAddr().c_str(), currTime, docid, env.getRemoteAddr().c_str(), currTime); 
    }
    else {
      //cout << body() << e.st_size;
      sprintf(cmd, "./XMLSignGenerator %s %s 0 %d /tmp/sig.%s.%lf 1 %s >& /tmp/%s.%lf.errorfile", XMLFILE.c_str(), XMLDTD.c_str(), IRRPOLYVAL, env.getRemoteAddr().c_str(), currTime, docid, env.getRemoteAddr().c_str(), currTime); 
    }
    //cout << body() << cmd << endl;
    //return 0;
    // Output the error file
    if (system(cmd) != 0) {
      cout << body() << br() << "<font color=\"red\">Error creating signature...</font>" << endl;
      string line;
      char errFile[128];
      sprintf(errFile, "/tmp/%s.%lf.errorfile", env.getRemoteAddr().c_str(), currTime);
      ifstream myfile (errFile);
      cout << body() << "<font color=\"red\">";
      if (myfile.is_open())
      {
        while (! myfile.eof() )
        {
          getline (myfile,line);
          cout << body() << br() << line << endl;
        }
        myfile.close();
      }
      cout << body() << "</font>";
      cout << body() << html() << endl;
      return 0;
    }
    else {
      //cout << body() << br() << "Successfully created signature on UMKC webserver." << endl;
    }

    const char *BOOTHOST = pickHost();
    cout << BOOTHOST << br() << endl;
    if (!BOOTHOST) {
      cout << "Unable to find a PlanetLab node..." << endl;
      cout << body() << html() << endl; 
      return 0;
    }
    const char *REMOTEINSTALLDIR = getenv("REMOTEINSTALLDIR");
    const char *MAXREPLICAS = getenv("MAXREPLICAS");

    // Publishing
    /*
    sprintf(cmd, "scp -r -i ./id_rsa /tmp/sig.%s.%lf* umkc_rao@%s:%s >& /tmp/%s.%lf.ssherrorfile", env.getRemoteAddr().c_str(), currTime, BOOTHOST, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime);

    if (system(cmd) != 0) {
      cout << "<font color=\"red\">Failed to copy to PlanetLab node. Please retry. </font>" << BOOTHOST << endl;
      cout << body() << html() << endl;
      return 0;
    }
    else {
      cout << br() << "Successful copy to PlanetLab node: " << BOOTHOST << endl;
    }
   
    // Publish the document -- OLD version
    sprintf(cmd, "ssh -i ./id_rsa umkc_rao@%s %s/bin/psi %s/tmp/lsd-1/dhash-sock -IS %s/sig.%s.%lf 0 1 0 1 0 %s >& /var/www/html/raopr/logs/%s.%lf.publishfile &", BOOTHOST, REMOTEINSTALLDIR, REMOTEINSTALLDIR, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, MAXREPLICAS, env.getRemoteAddr().c_str(), currTime);
    
    */
    // Compress to create one file
    sprintf(cmd, "cd /tmp; tar cvfz sig.%s.%lf.tar.gz sig.%s.%lf* > /dev/null; cd - >/dev/null", env.getRemoteAddr().c_str(), currTime, 
            env.getRemoteAddr().c_str(), currTime);
    if (system(cmd) != 0) {
      cout << "Problem...";
      return 0;
    }

    sprintf(cmd, "ssh -i ./id_rsa umkc_rao@%s cat < /tmp/sig.%s.%lf.tar.gz \"> %s/sig.%s.%lf.tar.gz ; cd %s; tar xvfz sig.%s.%lf.tar.gz > /dev/null; %s/bin/psi %s/tmp/lsd-1/dhash-sock -IS %s/sig.%s.%lf 0 1 0 1 0 %s \" >& /var/www/html/raopr/logs/%s.%lf.publishfile & ", BOOTHOST, env.getRemoteAddr().c_str(), currTime, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, REMOTEINSTALLDIR, REMOTEINSTALLDIR, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, MAXREPLICAS, env.getRemoteAddr().c_str(), currTime);
    //cout << body() << br() << cmd;

    if (system(cmd) != 0) {
      cout << body() << br() << "<font color=\"red\">Failed to publish from PlanetLab node: </font>" << BOOTHOST;
    }
    else {
      sprintf(cmd, "Check/refresh the <a href=\"http://vortex.sce.umkc.edu/raopr/logs/%s.%lf.publishfile\">log file</a> for the status. The message \"<font color=\"blue\">TASK COMPLETED</font> \" in the log file denotes the completion of publishing. <br> <br> <br> <br> <a href=\"http://vortex.sce.umkc.edu/psix/publish.html\">[Back to Publishing]</a> ", env.getRemoteAddr().c_str(), currTime);
      cout << body() << br() << br() << cmd;
    }    
    // Close the document
    cout << body() << html() << endl;
  }
  
  catch(const exception& e) {
    // handle error condition
  }
  
  return 0;
}
