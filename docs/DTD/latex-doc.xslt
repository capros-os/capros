<?xml version="1.0" encoding="UTF-8"?>
<!--
 Transformer from the test.dtd input to well-formed LaTeX
 (if there is such a thing)...
-->

<xsl:stylesheet
  version ="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/TR/REC-html40">

  <xsl:output method="text" indent="no"/>
  <xsl:strip-space elements="*"/>
  <xsl:preserve-space elements="text()"/>

  <!-- Things that are universal: -->

  <xsl:template match="a">
    <!-- FIX: This needs to be hacked to generate page references -->
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="figure">
    <xsl:text>\begin{figure}[htb]
  \begin{center}
    \leavevmode
    \psfig{figure=</xsl:text><xsl:apply-templates select="@src"/>.eps<xsl:text>}
  \end{center}
\end{figure}
</xsl:text>
  </xsl:template>

  <!-- Footnote content model: -->

  <xsl:template match="fn-em">
    <xsl:text>\emph{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="fn-b">
    <xsl:text>\textbf{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="fn-code">
    <xsl:text>\texttt{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="fn-p">
<xsl:apply-templates/>
<xsl:text>

    </xsl:text>
  </xsl:template>

  <xsl:template match="fn-ul">
    <xsl:text>\begin{itemize}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{itemize}</xsl:text>
  </xsl:template>

  <xsl:template match="fn-ol">
    <xsl:text>\begin{enumerate}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{enumerate}</xsl:text>
  </xsl:template>

  <xsl:template match="fn-li">
    <xsl:text>
\item </xsl:text><xsl:apply-templates/>
  </xsl:template>


  <!-- Title content model: -->

  <xsl:template match="h-em">
    <xsl:text>\emph{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="h-b">
    <xsl:text>\textbf{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="h-code">
    <xsl:text>\texttt{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>


  <!-- Body content model (basic pieces): -->

  <xsl:template match="em">
    <xsl:text>\emph{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>
  <xsl:template match="b">
    <xsl:text>\textbf{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>
  <xsl:template match="code">
    <xsl:text>\begin{texttt}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{texttt}</xsl:text>
  </xsl:template>
  <xsl:template match="sup">
    XXX-SUP-XXX<xsl:apply-templates/>XXX-ENDSUP-XXX
  </xsl:template>
  <xsl:template match="sub">
    XXX-SUB-XXX<xsl:apply-templates/>XXX-ENDSUB-XXX
  </xsl:template>

  <xsl:template match="gripe">
    <!-- This is broken -->
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="span">
    <!-- This is broken -->
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="defn">
    <xsl:text>\defn{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="tool">
    <xsl:text>\emph{\texttt{</xsl:text><xsl:apply-templates/><xsl:text>}}</xsl:text>
  </xsl:template>

  <xsl:template match="program">
    <!-- want underline -->
    <xsl:text>\texttt{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="method">
    <xsl:text>\emph{\texttt{</xsl:text><xsl:apply-templates/><xsl:text>}}</xsl:text>
  </xsl:template>

  <xsl:template match="docname">
    <xsl:text>\emph{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="term">
    <xsl:text>\myterm{</xsl:text><xsl:apply-templates/><xsl:text>}</xsl:text>
  </xsl:template>

  <xsl:template match="br">
    <xsl:text>

</xsl:text>
  </xsl:template>

  <xsl:template match="ul">
    <xsl:text>\begin{itemize}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{itemize}</xsl:text>
  </xsl:template>
  <xsl:template match="ol">
    <xsl:text>\begin{enumerate}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{enumerate}</xsl:text>
  </xsl:template>
  <xsl:template match="dl">
    <!-- FIX me this is wrong! -->
    <xsl:text>\begin{description}</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>\end{description}</xsl:text>
  </xsl:template>
  <xsl:template match="li">
    <xsl:text>
\item </xsl:text>
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="dlnm">
    <xsl:text>
\item[</xsl:text>
    <xsl:apply-templates/>
    <xsl:text>] </xsl:text>
  </xsl:template>

  <xsl:template match="dldescrip">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="p">
<xsl:apply-templates/>
<xsl:text>

    </xsl:text>
  </xsl:template>

  <xsl:template match="hr">
    XXX-HR-XXX
  </xsl:template>

  <xsl:template match="h1">
    <xsl:text>\section{</xsl:text><xsl:apply-templates/><xsl:text>}
</xsl:text>
  </xsl:template>

  <xsl:template match="pp.introduction|pp.toedescrip|pp.securityenvironment|pp.securityobjectives|pp.itsecurityrequirements">
    <xsl:choose>
      <xsl:when test="name()='pp.disclaimer'">
    <xsl:text>\section{Disclaimer}
</xsl:text>
      </xsl:when>
        <xsl:when test="name()='pp.introduction'">
    <xsl:text>\section{Introduction}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.toedescrip'">
    <xsl:text>\section{TOE Description}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.securityenvironment'">
    <xsl:text>\section{Security Environment}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.securityobjectives'">
    <xsl:text>\section{Security Objectives}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.itsecurityrequirements'">
    <xsl:text>\section{Security Requirements}
</xsl:text>
      </xsl:when>
    </xsl:choose>
<xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="h2">
    <xsl:text>\subsection{</xsl:text><xsl:apply-templates/><xsl:text>}
</xsl:text>
  </xsl:template>

  <xsl:template match="pp.se.assumptions|pp.se.threats|pp.se.policies|pp.so.toe|pp.so.environment|pp.so.rationale|pp.it.functional|pp.it.assurance">
    <xsl:choose>
      <xsl:when test="name()='pp.se.assumptions'">
    <xsl:text>\subsection{Assumptions}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.se.threats'">
    <xsl:text>\subsection{Threats}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.se.policies'">
    <xsl:text>\subsection{Security Policies}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.so.toe'">
    <xsl:text>\subsection{Security Objectives for the TOE}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.so.environment'">
    <xsl:text>\subsection{Security Objectives for the Environment}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.so.rationale'">
    <xsl:text>\subsection{Conclusions and Security Objectives Rationale}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.it.functional'">
    <xsl:text>\subsection{Functional Requirements}
</xsl:text>
      </xsl:when>
      <xsl:when test="name()='pp.it.assurance'">
    <xsl:text>\subsection{Assurance Requirements}
</xsl:text>
      </xsl:when>
    </xsl:choose>
<xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="h3">
    <xsl:text>\subsubsection{</xsl:text><xsl:apply-templates/><xsl:text>}
</xsl:text>
  </xsl:template>

  <xsl:template match="h4">
    <xsl:text>\paragraph{</xsl:text><xsl:apply-templates/><xsl:text>}
</xsl:text>
  </xsl:template>

  <xsl:template match="warn">
    <xsl:text>
\begin{indent}
\textbf{Warning:}

</xsl:text>
<xsl:apply-templates/>
<xsl:text>
\end{indent}
</xsl:text>
  </xsl:template>

  <xsl:template match="note">
    <xsl:text>
\begin{indent}
\textbf{Note:}

</xsl:text>
<xsl:apply-templates/>
<xsl:text>
\end{indent}
</xsl:text>
  </xsl:template>

  <xsl:template match="caution">
    <xsl:text>
\begin{indent}
\textbf{Caution:}

</xsl:text>
<xsl:apply-templates/>
<xsl:text>
\end{indent}
</xsl:text>
  </xsl:template>

  <xsl:template match="footnote">\footnote{<xsl:apply-templates/>}</xsl:template>

  <xsl:template match="pp.issues|pp.objectives">
\begin{trivlist}
\setlength{\parindent}{0in}%
\setlength{\itemindent}{-12pt}%
\advance\leftskip 24pt%
\advance\linewidth -24pt
\makeatletter
\advance\@totalleftmargin 24pt
\makeatother

    <xsl:apply-templates/>
\advance\leftskip -24pt%
\advance\linewidth 24pt
\makeatletter
\advance\@totalleftmargin -24pt
\makeatother
\end{trivlist}
  </xsl:template>

  <xsl:template match="pp.description">
\item[\textbf{<xsl:apply-templates select="../@name"/>}]
    <xsl:apply-templates select="p[1]/*|p[1]/text()"/>

    <xsl:apply-templates select="ol"/>
  </xsl:template>

  <xsl:template match="pp.description//p">
<xsl:apply-templates/>
    <xsl:text>

</xsl:text>
  </xsl:template>

  <xsl:template match="pp.notes">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="pp.sfrs">
\begin{trivlist}
\setlength{\parindent}{0in}%
\setlength{\itemindent}{-12pt}%
\advance\leftskip 24pt%
\advance\linewidth -24pt
\makeatletter
\advance\@totalleftmargin 24pt
\makeatother

    <xsl:apply-templates/>
\advance\leftskip -24pt%
\advance\linewidth 24pt
\makeatletter
\advance\@totalleftmargin -24pt
\makeatother
\end{trivlist}
  </xsl:template>

  <xsl:template match="pp.objective|pp.sfr|pp.issue">
    <xsl:apply-templates select="pp.description"/>
    <xsl:apply-templates select="pp.notes"/>
    <xsl:if test="count(.//pp.issue.ref) != 0">
      <xsl:text>

\textbf{Correlations:} </xsl:text>
      <xsl:for-each select="pp.issue.ref">
	<xsl:if test="position() &gt; 1">
	  <xsl:text>, </xsl:text>
	</xsl:if>
	<xsl:apply-templates select="@name"/>
      </xsl:for-each>
    </xsl:if>

    <!-- Dependencies are not currently useful, so turn them off: -->
    <xsl:if test="count(.//pp.sfr.dependency) != 0">
      <xsl:text>

\textbf{Depends on:} </xsl:text>
      <xsl:for-each select="pp.sfr.dependency">
	<!-- Verify that the dependency is satisfied! -->
	
	<xsl:if test="position() &gt; 1">
	  <xsl:text>, </xsl:text>
	</xsl:if>
	<xsl:apply-templates select="@name"/>
      </xsl:for-each>
    </xsl:if>
  </xsl:template>

  <!-- Body content model (tables): -->
  <!-- FIX: This all needs serious work. -->

  <xsl:template match="table">
    <xsl:element name="{name()}">
      <xsl:attribute name="cellpadding">10</xsl:attribute>
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
      <xsl:apply-templates/>
    </xsl:element>
  </xsl:template>

  <xsl:template match="tbody|tr|td">
    <xsl:element name="{name()}">
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
      <xsl:apply-templates/>
    </xsl:element>
  </xsl:template>


  <!-- Back matter content model: -->
  <!-- fix up citations! -->

  <xsl:template match="bibliography">
    <xsl:text>
\begin{thebibliography}{999}
</xsl:text>
    <xsl:apply-templates select="citation"/>
\end{thebibliography}
  </xsl:template>

  <xsl:template match="citation">
    <xsl:text>\bibitem{</xsl:text>
    <xsl:apply-templates select="@name"/>
    <xsl:text>}
    </xsl:text>
    <xsl:for-each select="cite.author">
      <xsl:choose>
	<xsl:when test="position() = last() and position() != 1">
	  <xsl:text> and </xsl:text>
	</xsl:when>
	<xsl:when test="position() &gt; 1">
	  <xsl:text>, </xsl:text>
	</xsl:when>
      </xsl:choose>
      <xsl:apply-templates/>
    </xsl:for-each>
    <xsl:text>, </xsl:text>
    <xsl:choose>
      <xsl:when test="count(./cite.journal|./cite.proceedings) != 0">
	<xsl:text>``</xsl:text>
	<xsl:apply-templates select="cite.title"/>
	<xsl:text>,'' </xsl:text>
	<xsl:apply-templates select="cite.journal|cite.proceedings"/>
      </xsl:when>
      <xsl:otherwise>
	<em>
	<xsl:apply-templates select="cite.title"/>
	</em>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:apply-templates select="cite.publisher"/>
    <xsl:apply-templates select="cite.volume"/>
    <xsl:apply-templates select="cite.number"/>
    <xsl:apply-templates select="cite.part"/>
    <xsl:apply-templates select="cite.month"/>
    <xsl:apply-templates select="cite.year"/>
    <xsl:apply-templates select="cite.pages"/>
    <xsl:apply-templates select="cite.url"/>
    <xsl:apply-templates select="cite.note"/>

    <xsl:text>

    </xsl:text>
  </xsl:template>


  <xsl:template match="cite.journal|cite.proceedings">
    <!-- Exceptional case - no comma insertion! -->
    \emph{<xsl:apply-templates/>}
  </xsl:template>

  <xsl:template match="cite.year|cite.month">
    <xsl:text>, </xsl:text><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="cite.volume">
    <xsl:text>, </xsl:text>\textbf{<xsl:apply-templates/>}
  </xsl:template>

  <xsl:template match="cite.number">
    <xsl:text>(</xsl:text><xsl:apply-templates/><xsl:text>)</xsl:text>
  </xsl:template>

  <xsl:template match="cite.part">
    <xsl:text>, part </xsl:text><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="cite.pages">
    <xsl:text>, pp. </xsl:text><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="cite.publisher">
    <xsl:text>, </xsl:text><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="cite.url">
    <xsl:text>, </xsl:text>\texttt{<xsl:apply-templates/>}
  </xsl:template>

  <xsl:template match="cite">
    <xsl:text>\cite{</xsl:text>
    <xsl:apply-templates select="@name"/>
    <xsl:text>}</xsl:text>
  </xsl:template>

  <!-- Overall content model: -->

  <xsl:template match="/">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="doc">
    <xsl:apply-templates select="front"/>
\begin{document}
\mytitle
    <xsl:apply-templates select="body"/>
    <xsl:apply-templates select="back"/>
<!--      <xsl:apply-templates select="copyright"/> -->
\end{document}
  </xsl:template>

  <xsl:template match="ProtectionProfile">
    <xsl:apply-templates select="pp.front"/>
\begin{document}
\mytitle

    <xsl:apply-templates select="pp.introduction"/>
    <xsl:apply-templates select="pp.toedescrip"/>
    <xsl:apply-templates select="pp.securityenvironment"/>
    <xsl:apply-templates select="pp.securityobjectives"/>
    <xsl:apply-templates select="pp.itsecurityrequirements"/>
    <xsl:apply-templates select="back"/>
<!--      <xsl:apply-templates select="copyright"/> -->
\end{document}
  </xsl:template>

  <xsl:template match="front|pp.front">
\documentclass[10pt]{article}

\usepackage{times}
\usepackage{psfig}
%%\usepackage{goodfoot}

\newcommand{\leadin}[1]{\smallskip \noindent \textbf{#1}\hspace{0.1in}}
\newcommand{\myterm}[1]{\emph{#1}}
\newcommand{\defn}[1]{\textbf{#1}}
\newcommand{\email}[1]{\makeatletter #1\makeatother}

\setlength{\textheight}{8.5in}\setlength{\topmargin}{-0.25in}
\setlength{\oddsidemargin}{-.25in}\setlength{\evensidemargin}{-.25in}
\addtolength{\oddsidemargin}{17pt}\addtolength{\evensidemargin}{17pt}
\setlength{\textwidth}{6.5 in}

\parindent=0pt
\parskip=4pt
\flushbottom


\def\mytitle{

\begin{center}

<xsl:apply-templates/>

\medskip
\today\\
\medskip

<xsl:apply-templates select="../pp.disclaimer"/>
\end{center}
}
  </xsl:template>

  <xsl:template match="title">
\LARGE{\textbf{<xsl:value-of select="."/>}}\\
\medskip\normalsize
  </xsl:template>

  <xsl:template match="subtitle">
<xsl:value-of select="."/>\\
\medskip\normalsize
  </xsl:template>

  <xsl:template match="pp.front.ccversion">
\textbf{Criteria Version:} <xsl:apply-templates/>\\
  </xsl:template>

  <xsl:template match="pp.front.label">
\textbf{Protection Profile Label:} <xsl:apply-templates/>\\
  </xsl:template>

  <xsl:template match="pp.front.keywords">
\textbf{Keywords:} <xsl:apply-templates/>\\
  </xsl:template>

  <xsl:template match="draft">
    D R A F T
  </xsl:template>

  <xsl:template match="obgroup">
    <xsl:text>\subsection</xsl:text>
    <xsl:value-of select="."/>
    <xsl:text>}
</xsl:text>
  </xsl:template>

  <xsl:template match="operation">
  </xsl:template>

  <xsl:template match="opname">
  </xsl:template>

  <xsl:template match="key|word|string">
  </xsl:template>

  <xsl:template match="throws">
  </xsl:template>

  <xsl:template match="keytype">
  </xsl:template>

  <xsl:template match="author/name">
<xsl:apply-templates/>\\
  </xsl:template>

  <xsl:template match="author/organization">
<xsl:apply-templates/>\\
  </xsl:template>

  <xsl:template match="author/address">
<xsl:apply-templates/>\\
  </xsl:template>

  <xsl:template match="author/email">
\makeatletter\texttt{<xsl:apply-templates/>}\makeatother\\
\medskip\normalsize
  </xsl:template>

  <xsl:template match="abstract">
\begin{abstract}
    <xsl:apply-templates mode="abstract"/>
\end{abstract}
  </xsl:template>

  <xsl:template match="pp.disclaimer">
\newenvironment{disclaimer}
               {\small
                 \setlength{\parindent}{.15 in}%
                 \parskip=0pt%
                 \advance\leftskip .5in%
                 \advance\rightskip .5in%
                 \centerline{\textbf{Disclaimer}}%
%
                 \smallskip
}
               {\bigbreak}
\begin{disclaimer}

<xsl:apply-templates/>
\end{disclaimer}
  </xsl:template>

  <xsl:template match="copyright">
    <hr width="10%" align="left"/>
    <em>
      Copyright (C) 
      <xsl:apply-templates select="year"/>,
      <xsl:apply-templates select="organization"/>. All rights reserved.
      <xsl:apply-templates select="copy-terms"/>
    </em>
  </xsl:template>

  <xsl:template match="copy-terms">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="text()">
    <xsl:call-template name="clean-text">
      <xsl:with-param name="text">
	<xsl:value-of select="."/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="@*">
    <xsl:call-template name="clean-text">
      <xsl:with-param name="text">
	<xsl:value-of select="."/>
      </xsl:with-param>
    </xsl:call-template>
  </xsl:template>

  <xsl:template name="clean-text">
    <xsl:param name="text"/>
    <xsl:choose>
      <xsl:when test="contains($text, '_')">
	<xsl:call-template name="clean-text">
	  <xsl:with-param name="text">
	    <xsl:value-of select="substring-before($text, '_')"/>
	  </xsl:with-param>
	</xsl:call-template>
	<xsl:text>\_</xsl:text>
	<xsl:call-template name="clean-text">
	  <xsl:with-param name="text">
	    <xsl:value-of select="substring-after($text, '_')"/>
	  </xsl:with-param>
	</xsl:call-template>
      </xsl:when>
      <xsl:otherwise>
	<xsl:value-of select="$text"/>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>
</xsl:stylesheet>
		
