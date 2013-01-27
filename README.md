In this paper, we address the problem of cardinality estimation of XPath
queries over XML data stored in a distributed, Internet-scale
environment such as a large-scale, data sharing system designed to
foster innovations in biomedical and health informatics. The cardinality
estimate of XPath expressions is useful in XQuery optimization,
designing IR-style relevance ranking schemes, and statistical hypothesis
testing. We present a novel gossip algorithm called XGossip, which given
an XPath query, estimates the number of XML documents in the network
that contain a match for the query. XGossip is designed to be scalable,
decentralized, and robust to failures - properties that are desirable in
a large-scale distributed system. XGossip employs a novel
divide-and-conquer strategy for load balancing and reducing the
bandwidth consumption. We conduct theoretical analysis of XGossip in
terms of accuracy of cardinality estimation, message complexity, and
bandwidth consumption. We present a comprehensive performance evaluation
of XGossip on Amazon EC2 using a heterogeneous collection of XML
documents.
