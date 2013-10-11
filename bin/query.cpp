/*
 * $Id: example.cpp,v 1.3 2001/09/03 22:11:55 sbooth Exp $ 
 *
 * Skeleton of a CGI application written using the GNU cgicc library
 */
/*
 *  Praveen Rao, UMKC, 2008.
 * /

#include <exception>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
main(int argc, char **argv)
{
  try {
    Cgicc cgi;
    
    // Output the HTTP headers for an HTML document, and the HTML 4.0 DTD info
    cout << HTTPHTMLHeader() << HTMLDoctype(HTMLDoctype::eStrict) << endl;
    //cout << html().set("lang", "en").set("dir", "ltr") << endl;

    // Set up the page's header and title.
    cout << head() << endl;
    cout << title() << "psiX" << title() << endl;
    cout << head() << endl;
    
    // Start the HTML body
    cout << body() << endl;

    // Print out a message
    cout << h2("Locating documents under progress...") << endl;
    cout << h3("<font color=brown>Note that remote copy and SSH to some PlanetLab nodes can take more than a few seconds. Please be patient.</font>") << endl;

    // Content
    const CgiEnvironment& env = cgi.getEnvironment();
    
    char cmd[1024];

    double currTime = getgtod();
    char myTime[1024];
    sprintf(myTime, "%lf", currTime);

    string XPATHFILE = WORKINGDIR + env.getRemoteAddr() + string(myTime) + ".xpath";
    string XPATHDTD = WORKINGDIR + env.getRemoteAddr() + string(myTime) + ".dtd";
    sprintf(cmd, "rm -rf %s/*%s*", WORKINGDIR, env.getRemoteAddr().c_str());
    //cout << cmd;
    //system(cmd);

    string postData = env.getPostData(); 
    ofstream testfile("/tmp/junk");
    testfile << env.getPostData();
    testfile.close();
    //cout << body() << br() << env.getPostData();
    char *ptr = strstr(postData.c_str(), "\"xmlpath\"");
    char buf[1024];
    if (ptr != NULL) {
      sscanf(ptr+strlen("\"xmlpath\"")+strlen("\r\n\r\n"), "%[^\r\n]", buf);
    }
    //cout << body() << br() << "-->" << buf << "-->";
    // If empty actually
    if (strstr(buf, "--------------")) {
      buf[0] = 0;
    }

    if (strlen(buf) == 0) {
      cout << "XPath query is EMPTY" << endl;
      cout << body() << html() << endl;
      return 0;
    }
    ofstream xpathfile(XPATHFILE.c_str());
    xpathfile << buf;
    xpathfile.close();

    /* Does not work with input field "file" */
    /*const_form_iterator name = cgi.getElement("xmlpath");
    if (name != cgi.getElements().end()) { 
      cout << body() << **name;
      ofstream xpathfile(XPATHFILE.c_str());
      xpathfile << **name;
      xpathfile.close();
    }*/
    
    //else {
    //  cout << body() << "Could not get query...";
    //}

    const_file_iterator file = cgi.getFile("xmldtd");
    if (file != cgi.getFiles().end()) {
      ofstream xmldtd(XPATHDTD.c_str());
      (*file).writeToStream(xmldtd);
      xmldtd.close();
    }

    // Check the type of replica
    ptr = strstr(postData.c_str(), "\"replica\"");
    char replicaType[128];
    if (ptr != NULL) {
      sscanf(ptr+strlen("\"replica\"")+strlen("\r\n\r\n"), "%[^\r\n]", replicaType);
    }

    // Create a signature
    setenv("LD_LIBRARY_PATH", "./lib", 1);
    
    struct stat e;
    // Check if DTD is actually passed
    if (stat(XPATHDTD.c_str(), &e) == -1 || e.st_size == 0) {
      //cout << body() << br() << "Creating signatures without DTD" << endl;
      sprintf(cmd, "./XMLSignGenerator %s junk 1 %d /tmp/sig.%s.%lf 6 -1 >& /tmp/%s.%lf.errorfile", XPATHFILE.c_str(), IRRPOLYVAL, env.getRemoteAddr().c_str(), currTime, env.getRemoteAddr().c_str(), currTime); 
    }
    else {
      sprintf(cmd, "./XMLSignGenerator %s %s 1 %d /tmp/sig.%s.%lf 2 >& /tmp/%s.%lf.errorfile", XPATHFILE.c_str(), XPATHDTD.c_str(), IRRPOLYVAL, env.getRemoteAddr().c_str(), currTime, env.getRemoteAddr().c_str(), currTime); 
      //cout << body() << cmd;
    }

    // Output the error file
    if (system(cmd) != 0) {
      cout << br() << "<font color=\"red\">Error creating signature...</font>" << endl;
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
      //cout << body() << br() << "Successfully created signature..." << endl;
    }
	//cout << h2("testing") << endl; return 0;

    const char *BOOTHOST = pickHost();
    //char *HOSTLIST = getenv("HOSTLIST");
    cout << BOOTHOST << br() << endl;
    if (!BOOTHOST) {
      cout << br() << "Unable to find a PlanetLab node..." << endl;
      cout << body() << html() << endl; 
      return 0;
    }
    

    const char *REMOTEINSTALLDIR = getenv("REMOTEINSTALLDIR");
    const char *MAXREPLICAS = getenv("MAXREPLICAS");

    // Querying
    /*
    sprintf(cmd, "scp -r -i ./id_rsa /tmp/sig.%s.%lf* umkc_rao@%s:%s >& /tmp/%s.%lf.scperrorfile", env.getRemoteAddr().c_str(), currTime, BOOTHOST, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime);
//	cout << cmd << endl;
    if (system(cmd) != 0) {
      cout << br() << "<font color=\"red\">Failed to copy to PlanetLab node </font>" << BOOTHOST;
      cout << body() << html() << endl;
      return 0;
    }
    else {
      cout << "<font color=green> Successful copy to PlanetLab node: " << BOOTHOST << " </font>" << endl;
      cout << br() << "Performing SSH to Planetlab node " << BOOTHOST << " to issue query." << endl;
    }

    sprintf(cmd, "ssh -i ./id_rsa umkc_rao@%s %s/bin/psi %s/tmp/lsd-1/dhash-sock -O %s/sig.%s.%lf 0 1 0 1 %s %s >& /tmp/%s.%lf.queryfile", BOOTHOST, REMOTEINSTALLDIR, REMOTEINSTALLDIR, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, replicaType, MAXREPLICAS, env.getRemoteAddr().c_str(), currTime);
    */

    // Compress to create one file
    sprintf(cmd, "cd /tmp; tar cvfz sig.%s.%lf.tar.gz sig.%s.%lf* > /dev/null; cd - >/dev/null", env.getRemoteAddr().c_str(), currTime, 
            env.getRemoteAddr().c_str(), currTime);
    if (system(cmd) != 0) {
      cout << "Problem...";
      return 0;
    }
    // Run the query
    sprintf(cmd, "ssh -i ./id_rsa umkc_rao@%s cat < /tmp/sig.%s.%lf.tar.gz \"> %s/sig.%s.%lf.tar.gz ; cd %s; tar xvfz sig.%s.%lf.tar.gz > /dev/null; %s/bin/psi %s/tmp/lsd-1/dhash-sock -O %s/sig.%s.%lf 0 1 0 1 %s %s \" >& /tmp/%s.%lf.queryfile", BOOTHOST, env.getRemoteAddr().c_str(), currTime, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, REMOTEINSTALLDIR, REMOTEINSTALLDIR, REMOTEINSTALLDIR, env.getRemoteAddr().c_str(), currTime, replicaType, MAXREPLICAS, env.getRemoteAddr().c_str(), currTime);
    //cout << body() << br() << cmd;
    if (system(cmd) != 0) {
      cout << br() << "<font color=\"red\">Failed to query from PlanetLab node</font> " << BOOTHOST;
    }
    else {
      //cout << br() << "Successfully issued the query from Planetlab node " << BOOTHOST << endl;
      string line;
      char fileName[128];

      sprintf(cmd, "grep 'Query time' /tmp/%s.%lf.queryfile >> /tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), env.getRemoteAddr().c_str(), currTime);
      system(cmd);

      sprintf(cmd, "echo \"<br><u>Docids of matching documents are listed below. <br> You can contact the publishers for the XML documents.</u>\" >> /tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), currTime);
      system(cmd);

      sprintf(cmd, "grep 'Docid' /tmp/%s.%lf.queryfile >> /tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), currTime, env.getRemoteAddr().c_str(), currTime);
      system(cmd);

      sprintf(cmd, "grep 'Num docs' /tmp/%s.%lf.queryfile >> /tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), currTime, env.getRemoteAddr().c_str(), currTime);
      system(cmd);

      sprintf(cmd, "echo \"<br><u>HOPS required to access each H-index node.</u>\" >> /tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), currTime);
      system(cmd);
      sprintf(cmd, "grep 'HOPS' /tmp/%s.%lf.queryfile >> /tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), currTime, env.getRemoteAddr().c_str(), currTime);
      system(cmd);

      sprintf(fileName, "/tmp/%s.%lf.queryout", env.getRemoteAddr().c_str(), currTime);
      ifstream myfile (fileName);
      cout << body() << "<hr><font color=\"blue\">";
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
    } 
    sprintf(cmd, "<br> <br> <br> <br> <a href=\"http://vortex.sce.umkc.edu/psix/query.html\">[Back to Querying]</a> ");
    cout << body() << cmd;

    // Close the document
    cout << body() << html() << endl;
  }
  
  catch(const exception& e) {
    // handle error condition
  }
  
  return 0;
}
