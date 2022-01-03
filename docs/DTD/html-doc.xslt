<?xml version="1.0" encoding="UTF-8"?>
<!--
 Transformer from the test.dtd input to well-formed HTML..
-->

<xsl:stylesheet
  version ="1.0"
  xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
  xmlns="http://www.w3.org/TR/REC-html40">

  <xsl:output method="html" indent="yes"/>

  <xsl:param name="docname"/>

  <xsl:variable name="h1_index">
    <xsl:number value="0"/>
  </xsl:variable>
  <xsl:variable name="h2_index">
    <xsl:number value="0"/>
  </xsl:variable>
  <xsl:variable name="h3_index">
    <xsl:number value="0"/>
  </xsl:variable>

  <!-- For some reason this doesn't work: -->
  <xsl:variable name="CSS_STYLESHEET">
    <style type="text/css">
          <![CDATA[<!--]]>
A { text-decoration: none }
A:visited { text-decoration: none; color: blue }
A:hover { text-decoration: none; color: red }
A:active { text-decoration: none; color: purple } 
H1 { 	
        font-size: large;
	font-family: sans-serif;
	margin-top: 12pt;
	margin-bottom: 12pt;
} 
H1.title {
        font-size: x-large;
}
H2 {
	text-decoration: underline;
	font-weight: normal;
	text-indent: 0;
	margin-top: 12pt;
	font-size: large;
	font-family: sans-serif;
}

H3 {
	text-indent: 0;
	margin-top: 12pt;
	font-style: italic;
	font-family: sans-serif;
/*	font-size: large; */
/*	font-weight: normal; */

}

UL { list-style-type: disc; }
UL UL { list-style-type: square; }
OL OL { list-style-type: upper-alpha; }

SPAN.gripe {
        color: red;
	text-decoration: underline;
}
          <![CDATA[-->]]>
        </style>
  </xsl:variable>

  <!-- Things that are universal: -->

  <xsl:template match="a">
    <xsl:element name="a">
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
    <xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="figure">
    <xsl:element name="img">
      <xsl:attribute name="src"><xsl:value-of select="@src"/>.gif</xsl:attribute>
      <xsl:attribute name="align"><xsl:value-of select="@align"/></xsl:attribute>
      <xsl:attribute name="alt"><xsl:value-of select="@alt"/></xsl:attribute>
    </xsl:element>
  </xsl:template>

  <!-- Footnote content model: -->

  <xsl:template match="fn-em">
    <xsl:element name="em"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="fn-b">
    <xsl:element name="b"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="fn-code">
    <xsl:element name="code"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="fn-p">
    <xsl:element name="p"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="fn-ul">
    <xsl:element name="ul"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="fn-ol">
    <xsl:element name="code"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="fn-li">
    <xsl:element name="code"><xsl:apply-templates/></xsl:element>
  </xsl:template>


  <!-- Title content model: -->

  <xsl:template match="h-em">
    <xsl:element name="em"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="h-b">
    <xsl:element name="b"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="h-code">
    <xsl:element name="code"><xsl:apply-templates/></xsl:element>
  </xsl:template>


  <!-- Body content model (basic pieces): -->

  <xsl:template match="em|b|code|sup|sub">
    <xsl:element name="{name()}"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="gripe">
    <span class="gripe"><xsl:apply-templates/></span>
  </xsl:template>

  <xsl:template match="span">
    <xsl:element name="span">
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
      <xsl:apply-templates/>
    </xsl:element>
  </xsl:template>

  <xsl:template match="defn">
    <xsl:element name="b"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="tool">
    <xsl:element name="em"><xsl:element name="tt"><xsl:apply-templates/></xsl:element></xsl:element>
  </xsl:template>

  <xsl:template match="program">
    <xsl:element name="ul"><xsl:element name="font"><xsl:attribute name="color">blue</xsl:attribute><xsl:element name="tt"><xsl:apply-templates/></xsl:element></xsl:element></xsl:element>
  </xsl:template>

  <xsl:template match="method">
    <xsl:element name="em"><xsl:element name="font"><xsl:attribute name="color">brown</xsl:attribute><xsl:element name="tt"><xsl:apply-templates/></xsl:element></xsl:element></xsl:element>
  </xsl:template>

  <xsl:template match="docname">
    <xsl:element name="em"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="term">
    <xsl:element name="em"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="br">
    <xsl:element name="{name()}">
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
      <xsl:apply-templates/>
    </xsl:element>
  </xsl:template>

  <xsl:template match="ul|ol|dl|li|p">
    <xsl:element name="{name()}">
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
      <xsl:apply-templates/>
    </xsl:element>
  </xsl:template>

  <xsl:template match="dlnm">
    <xsl:element name="dt"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="dldescrip">
    <xsl:element name="dd"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="hr">
    <xsl:element name="{name()}">
      <xsl:for-each select="@*">
        <xsl:attribute name="{name()}"><xsl:value-of select="."/></xsl:attribute>
      </xsl:for-each>
      <xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="p" mode="abstract">
    <xsl:element name="{name()}"><em><xsl:apply-templates/></em></xsl:element>
  </xsl:template>

  <xsl:template match="h1">
    <xsl:variable name="h1_index">
      <xsl:number value="1+count(preceding::h1|preceding::pp.introduction|preceding::pp.toedescrip|preceding::pp.securityenvironment|preceding::pp.securityobjectives|preceding::pp.itsecurityrequirements)"/>
    </xsl:variable>
    <h1><xsl:value-of select="$h1_index"/>. <xsl:apply-templates/></h1>
  </xsl:template>

  <xsl:template match="pp.introduction|pp.toedescrip|pp.securityenvironment|pp.securityobjectives|pp.itsecurityrequirements">
    <xsl:variable name="h1_index">
      <xsl:number value="1+count(preceding::h1|preceding::pp.introduction|preceding::pp.toedescrip|preceding::pp.securityenvironment|preceding::pp.securityobjectives|preceding::pp.itsecurityrequirements)"/>
    </xsl:variable>
    <xsl:variable name="h1_text">
      <xsl:choose>
        <xsl:when test="name()='pp.disclaimer'">
	  <xsl:text>Disclaimer</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.introduction'">
	  <xsl:text>Introduction</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.toedescrip'">
	  <xsl:text>TOE Description</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.securityenvironment'">
	  <xsl:text>Security Environment</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.securityobjectives'">
	  <xsl:text>Security Objectives</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.itsecurityrequirements'">
	  <xsl:text>IT Security Requirements</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text></xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <h1><xsl:value-of select="$h1_index"/>. <xsl:value-of select="$h1_text"/></h1><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="h2">
    <xsl:variable name="h2_index">
      <xsl:number value="1+count(preceding-sibling::h2|preceding-sibling::pp.se.assumptions|preceding-sibling::pp.se.threats|preceding-sibling::pp.se.policies|preceding-sibling::pp.so.toe|preceding-sibling::pp.so.environment|preceding-sibling::pp.so.rationale|preceding-sibling::pp.it.functional|preceding-sibling::pp.it.assurance)"/>
    </xsl:variable>
    <h2><xsl:value-of select="$h1_index"/>.<xsl:value-of select="$h2_index"/>. <xsl:apply-templates/></h2>
  </xsl:template>

  <xsl:template match="pp.se.assumptions|pp.se.threats|pp.se.policies|pp.so.toe|pp.so.environment|pp.so.rationale|pp.it.functional|pp.it.assurance">
    <xsl:variable name="h2_index">
      <xsl:number value="1+count(preceding-sibling::h2|preceding-sibling::pp.se.assumptions|preceding-sibling::pp.se.threats|preceding-sibling::pp.se.policies|preceding-sibling::pp.so.toe|preceding-sibling::pp.so.environment|preceding-sibling::pp.so.rationale|preceding-sibling::pp.it.functional|preceding-sibling::pp.it.assurance)"/>
    </xsl:variable>
    <xsl:variable name="h2_text">
      <xsl:choose>
        <xsl:when test="name()='pp.se.assumptions'">
	  <xsl:text>Assumptions</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.se.threats'">
	  <xsl:text>Threats</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.se.policies'">
	  <xsl:text>Security Policies</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.so.toe'">
	  <xsl:text>Security Objectives for the TOE</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.so.environment'">
	  <xsl:text>Security Objectives for the Environment</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.so.rationale'">
	  <xsl:text>Conclusions and Security Objectives Rationale</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.it.functional'">
	  <xsl:text>Functional Requirements</xsl:text>
	</xsl:when>
        <xsl:when test="name()='pp.it.assurance'">
	  <xsl:text>Assurance Requirements</xsl:text>
	</xsl:when>
	<xsl:otherwise>
	  <xsl:text></xsl:text>
	</xsl:otherwise>
      </xsl:choose>
    </xsl:variable>
    <h2><xsl:value-of select="$h1_index"/>.<xsl:value-of select="$h2_index"/>. <xsl:value-of select="$h2_text"/></h2><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="h3">
    <xsl:variable name="h3_index">
      <xsl:number value="1+count(preceding-sibling::h3)"/>
    </xsl:variable>
    <h3><xsl:value-of select="$h1_index"/>.<xsl:value-of select="$h2_index"/>.<xsl:value-of select="$h3_index"/>. <xsl:apply-templates/></h3>
  </xsl:template>

  <xsl:template match="h4">
    <h4><xsl:apply-templates/></h4>
  </xsl:template>

  <xsl:template match="warn">
    <ul>
      <p><b>Warning:</b></p>
      <xsl:apply-templates/>
    </ul>
  </xsl:template>

  <xsl:template match="note">
    <ul>
    <p><b>Note:</b></p>
      <xsl:apply-templates/>
    </ul>
  </xsl:template>

  <xsl:template match="caution">
    <ul>
    <p><b>Caution:</b></p>
      <xsl:apply-templates/>
    </ul>
  </xsl:template>

  <xsl:template match="footnote">
    <xsl:variable name="footnoteNumber">
      <xsl:number level="any"/>
    </xsl:variable>
    <sup><a href="#fn-{$footnoteNumber}"><xsl:value-of select="$footnoteNumber"/></a></sup>
  </xsl:template>

  <xsl:template match="footnote" mode="footnote">
    <xsl:variable name="footnoteNumber">
      <xsl:number level="any"/>
    </xsl:variable>
    <tr valign="top">
      <td>
        <a name="fn-{$footnoteNumber}"><xsl:value-of select="$footnoteNumber"/>.</a>
      </td>
      <td>
        <xsl:apply-templates/>
      </td>
    </tr>
  </xsl:template>

  <xsl:template match="pp.issues">
    <div class="indent">
      <xsl:apply-templates/>
    </div>
  </xsl:template>

  <xsl:template match="pp.description">
    <p class="hanging">
      <em>
	<b>
          <xsl:element name="a">
	    <xsl:attribute name="name">
	      <xsl:apply-templates select="../@name"/>
	    </xsl:attribute>
	    <xsl:value-of select="../@name"/>
	  </xsl:element>
	</b>
	<xsl:text> </xsl:text>
	<xsl:apply-templates select="p[1]/*|p[1]/text()"/>
      </em>
    </p>
    <xsl:apply-templates select="ol"/>
  </xsl:template>

  <xsl:template match="pp.description//p">
    <p>
      <em>
	<xsl:apply-templates/>
      </em>
    </p>
  </xsl:template>

  <xsl:template match="pp.notes">
    <xsl:apply-templates mode="pp.notes"/>
  </xsl:template>

  <xsl:template match="OLD.pp.issue">
    <xsl:variable name="h3_index">
      <xsl:number value="count(preceding-sibling::h3)"/>
    </xsl:variable>
    <p>
      <b>
        <xsl:value-of select="$h1_index"/>
        <xsl:text>.</xsl:text>
        <xsl:value-of select="$h2_index"/>
        <xsl:text>.</xsl:text>
        <xsl:value-of select="$h3_index"/>
        <xsl:text>.</xsl:text>
        <xsl:number value="1+count(preceding-sibling::pp.issue)"/>
        <xsl:text> </xsl:text>
        <xsl:value-of select="@name"/>:
      </b>
      <em><xsl:apply-templates/></em>
    </p>
  </xsl:template>

  <xsl:template match="pp.objectives">
    <div class="indent">
      <xsl:apply-templates/>
    </div>
  </xsl:template>

  <xsl:template match="pp.sfrs">
    <xsl:variable name="myname">
      <xsl:value-of select="@name"/>
    </xsl:variable>
    <xsl:if test="count(//pp.sfrs.superscedes[@name=$myname]) != 0">
      <xsl:message>
	<xsl:text>Warning: SFR Group </xsl:text>
	<xsl:value-of select="$myname"/> 
	<xsl:text> is supersceded by </xsl:text>
	<xsl:for-each select="//pp.sfrs[pp.sfrs.superscedes/@name=$myname]">
	  <xsl:if test="position() != 1">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:value-of select="@name"/>
	</xsl:for-each>
      </xsl:message>
    </xsl:if>
    <div class="indent">
      <xsl:element name="a">
	<xsl:attribute name="name">
	  <xsl:value-of select="@name"/>
	</xsl:attribute>
	<!-- no content, on purpose! -->
      </xsl:element>
      <xsl:for-each select="./pp.sfrs.superscedes">
	<xsl:element name="a">
	  <xsl:attribute name="name">
	    <xsl:value-of select="@name"/>
	  </xsl:attribute>
	  <!-- no content, on purpose! -->
	</xsl:element>
      </xsl:for-each>
      <xsl:apply-templates/>
    </div>
  </xsl:template>

  <xsl:template match="pp.objective|pp.sfr|pp.issue">
    <xsl:apply-templates select="pp.description"/>
    <xsl:apply-templates select="pp.notes"/>
    <xsl:if test="count(.//pp.issue.ref) != 0">
      <p>
	<b>Correlations:</b>
	<xsl:text> </xsl:text>
	<xsl:for-each select="pp.issue.ref">
	  <xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:element name="a">
	    <xsl:attribute name="href">
	      #<xsl:value-of select="@name"/>
	    </xsl:attribute>
	    <xsl:apply-templates select="@name"/>
	  </xsl:element>
	</xsl:for-each>
      </p>
    </xsl:if>
    <!-- Dependencies are not currently useful, so turn them off: -->
    <xsl:if test="count(.//pp.sfr.dependency) != 0">
      <p>
	<b>Depends on:</b>
	<xsl:text> </xsl:text>
	<xsl:for-each select="pp.sfr.dependency">
	  <!-- Verify that the dependency is satisfied! -->
	  <xsl:variable name="depname">
	    <xsl:value-of select="@name"/>
	  </xsl:variable>
	  <xsl:variable name="satcount">
	    <xsl:value-of select="count(//pp.sfrs[@name=$depname])+count(//pp.sfrs.superscedes[@name=$depname])"/>
	  </xsl:variable>
	  <xsl:if test="$satcount = 0">
	    <xsl:message>
	      <xsl:text>Warning: Dependency on </xsl:text>
	      <xsl:value-of select="$depname"/> 
	      <xsl:text> not satisfied.</xsl:text>
	    </xsl:message>
	  </xsl:if>
	  
	  <xsl:if test="position() &gt; 1">
	    <xsl:text>, </xsl:text>
	  </xsl:if>
	  <xsl:element name="a">
	    <xsl:attribute name="href">
	      #<xsl:value-of select="@name"/>
	    </xsl:attribute>
	    <xsl:apply-templates select="@name"/>
	  </xsl:element>
	</xsl:for-each>
      </p>
    </xsl:if>
  </xsl:template>

  <!-- Body content model (tables): -->

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
  <xsl:template name="citeref">
    <xsl:param name="tagname"/>
    <xsl:for-each select="//bibliography/citation">
      <xsl:sort select="cite.author"/>

      <xsl:if test="@name = $tagname">
        <xsl:value-of select="position()"/>
      </xsl:if>
    </xsl:for-each>
  </xsl:template>

  <xsl:template match="bibliography">
    <h1>Bibliography</h1>
    <table>
      <tbody>
        <xsl:apply-templates select="citation">
          <xsl:sort select="cite.author[1]"/>
        </xsl:apply-templates>
      </tbody>
    </table>
  </xsl:template>

  <xsl:template match="citation">
    <tr valign="top">
      <td>[<xsl:element name="a">
             <xsl:attribute name="name">
               <xsl:text>citation-</xsl:text>
               <xsl:apply-templates select="@name"/>
             </xsl:attribute>
             <xsl:value-of select="position()"/>
           </xsl:element>]
      </td>
      <td>
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
      </td>
    </tr>
  </xsl:template>

  <xsl:template match="cite.journal|cite.proceedings">
    <!-- Exceptional case - no comma insertion! -->
    <em><xsl:apply-templates/></em>
  </xsl:template>

  <xsl:template match="cite.year|cite.month">
    <xsl:text>, </xsl:text><xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="cite.volume">
    <xsl:text>, </xsl:text><b><xsl:apply-templates/></b>
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
    <xsl:text>, </xsl:text><xsl:element name="tt"><xsl:apply-templates/></xsl:element>
  </xsl:template>

  <xsl:template match="cite">
    <xsl:text>[</xsl:text>
    <xsl:element name="a">
      <xsl:attribute name="href">
        <xsl:text>#citation-</xsl:text>
        <xsl:apply-templates select="@name"/>
      </xsl:attribute>
      <xsl:call-template name="citeref">
        <xsl:with-param name="tagname" select="@name"/>
      </xsl:call-template>
    </xsl:element>
    <xsl:text>]</xsl:text>
  </xsl:template>


  <!-- Overall content model: -->

  <xsl:template match="/">
    <xsl:apply-templates/>
  </xsl:template>

  <xsl:template match="doc">
    <html>
      <head>
	<title><xsl:value-of select="front/title"/></title>
        <style type="text/css">
          <![CDATA[<!--]]>
A { text-decoration: none }
A:visited { text-decoration: none; color: blue }
A:hover { text-decoration: none; color: red }
A:active { text-decoration: none; color: purple } 
H1 { 	
        font-size: large;
	font-family: sans-serif;
	margin-top: 12pt;
	margin-bottom: 12pt;
} 
H1.title {
        font-size: x-large;
}
H2 {
	text-decoration: underline;
	font-weight: normal;
	text-indent: 0;
	margin-top: 12pt;
	font-size: large;
	font-family: sans-serif;
}

H3 {
	text-indent: 0;
	margin-top: 12pt;
	font-style: italic;
	font-family: sans-serif;
/*	font-size: large; */
/*	font-weight: normal; */
}

UL { list-style-type: disc; }
UL UL { list-style-type: square; }
OL OL { list-style-type: upper-alpha; }

SPAN.gripe {
        color: red;
	text-decoration: underline;
}
          <![CDATA[-->]]>
        </style>
      </head>
      <body bgcolor="#ffeedd" text="#000000" link="#0000ee"
	    vlink="#551a8b" alink="#ff0000">
        <table width="100%">
          <tbody>
            <tr valign="top">
              <td width="5%"><![CDATA[&nbsp;]]></td>
              <td width="90%">
                <xsl:apply-templates select="front"/>
                <xsl:apply-templates select="body"/>
		<xsl:if test="count(descendant::footnote)!=0">
		  <hr width="10%" align="left"/>
		  <b>Footnotes:</b>
                  <table width="100%">
		    <tbody>
		      <xsl:apply-templates select="descendant::footnote" mode="footnote"/>
		    </tbody>
		  </table>
		</xsl:if>
                <xsl:apply-templates select="back"/>
                <xsl:apply-templates select="copyright"/>
              </td>
              <td width="5%"><![CDATA[&nbsp;]]></td>
            </tr>
          </tbody>
        </table>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="obdoc">
    <html>
      <head>
	<title>EOR: <xsl:value-of select="title"/></title>
        <style type="text/css">
          <![CDATA[<!--]]>
A { text-decoration: none }
A:visited { text-decoration: none; color: blue }
A:hover { text-decoration: none; color: red }
A:active { text-decoration: none; color: purple } 
SPAN.gripe {
        color: red;
	text-decoration: underline;
}
          <![CDATA[-->]]>
        </style>
      </head>
      <body bgcolor="#ffeedd" text="#000000" link="#0000ee"
	    vlink="#551a8b" alink="#ff0000">
        <table width="100%">
          <tbody>
            <tr valign="top">
              <td width="5%"><![CDATA[&nbsp;]]></td>
              <td width="90%">
	        <div align="right">
                  <![CDATA[&nbsp;]]><br/>
                  <![CDATA[&nbsp;]]><br/>
                  <h2>EROS Object Reference</h2>
                  <xsl:apply-templates select="obgroup"/>
                </div>
                <xsl:apply-templates select="obname"/>
                <xsl:apply-templates select="draft"/>
                <ul>
                  <table>
                    <tbody>
                      <xsl:apply-templates select="keytype"/>
                    </tbody>
                  </table>
                </ul>
                <h3>Description</h3>
                <xsl:apply-templates select="description"/>
                <h3>Operations</h3>
                  <table>
                    <tbody>
                      <xsl:apply-templates select="operation"/>
                    </tbody>
                  </table>

		<xsl:if test="count(descendant::exception)!=0">
                  <h3>Exceptions</h3>
                  <xsl:apply-templates select="exception"/>
                </xsl:if>

		<xsl:if test="count(descendant::footnote)!=0">
		  <hr width="10%" align="left"/>
		  <b>Footnotes:</b>
                  <table width="100%">
		    <tbody>
		      <xsl:apply-templates select="descendant::footnote" mode="footnote"/>
		    </tbody>
		  </table>
		</xsl:if>

                <xsl:apply-templates select="back"/>
                <xsl:apply-templates select="copyright"/>
              </td>
              <td width="5%"><![CDATA[&nbsp;]]></td>
            </tr>
          </tbody>
        </table>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="ProtectionProfile">
    <html>
      <head>
	<title><xsl:value-of select="pp.front/title"/></title>
        <style type="text/css">
          <![CDATA[<!--]]>
P.hanging {
        text-indent: -5%;
}

DIV.indent {
        margin-left: 5%;
}

A { text-decoration: none }
A:visited { text-decoration: none; color: blue }
A:hover { text-decoration: none; color: red }
A:active { text-decoration: none; color: purple } 
H1 { 	
        font-size: large;
	font-family: sans-serif;
	margin-top: 12pt;
	margin-bottom: 12pt;
} 
H1.title {
        font-size: x-large;
}
H2 {
	text-decoration: underline;
	font-weight: normal;
	text-indent: 0;
	margin-top: 12pt;
	font-size: large;
	font-family: sans-serif;
}

H3 {
	text-indent: 0;
	margin-top: 12pt;
	font-style: italic;
	font-family: sans-serif;
	font-weight: bold;
/*	font-size: large; */
}

H4 {
	text-indent: 0;
	margin-top: 12pt;
	font-style: italic;
	font-family: sans-serif;
	font-weight: normal;
	text-decoration: underline;
/*	font-size: large; */
}

UL { list-style-type: disc; }
UL UL { list-style-type: square; }
OL OL { list-style-type: upper-alpha; }

P.box {
	  border-width: 1px;
}

SPAN.gripe {
        color: red;
}
SPAN.select {
        color: green;
	font-style: normal;
	font-family: sans-serif;
	text-decoration: underline;
}
SPAN.xselect {
        color: green;
	font-style: normal;
	font-family: sans-serif;
	font-style: italic;
}
SPAN.assign {
        color: assign;
	font-style: normal;
	font-family: sans-serif;
	text-decoration: underline;
}
SPAN.xassign {
        color: assign;
	font-style: normal;
	font-family: sans-serif;
	font-style: italic;
}
          <![CDATA[-->]]>
        </style>
      </head>
      <body bgcolor="#ffeedd" text="#000000" link="#0000ee"
	    vlink="#551a8b" alink="#ff0000">
        <table width="100%">
          <tbody>
            <tr valign="top">
              <td width="5%"><![CDATA[&nbsp;]]></td>
              <td width="90%">
		<p align="right" class="box">
		  <em>This document is also available as </em>
		  <xsl:element name="a">
		    <xsl:attribute name="href">
		      <xsl:value-of select="$docname"/><xsl:text>.pdf</xsl:text>
		    </xsl:attribute>
		    <xsl:text>PDF</xsl:text>
		  </xsl:element>.
		</p>
                <xsl:apply-templates select="pp.front"/>
                <xsl:apply-templates select="pp.disclaimer"/>
                <xsl:apply-templates select="pp.introduction"/>
                <xsl:apply-templates select="pp.toedescrip"/>
                <xsl:apply-templates select="pp.securityenvironment"/>
                <xsl:apply-templates select="pp.securityobjectives"/>
                <xsl:apply-templates select="pp.itsecurityrequirements"/>
		<xsl:if test="count(descendant::footnote)!=0">
		  <hr width="10%" align="left"/>
		  <b>Footnotes:</b>
                  <table width="100%">
		    <tbody>
		      <xsl:apply-templates select="descendant::footnote" mode="footnote"/>
		    </tbody>
		  </table>
		</xsl:if>
                <xsl:apply-templates select="back"/>
                <xsl:apply-templates select="copyright"/>
              </td>
              <td width="5%"><![CDATA[&nbsp;]]></td>
            </tr>
          </tbody>
        </table>
      </body>
    </html>
  </xsl:template>

  <xsl:template match="front">
    <center>
      <xsl:apply-templates/>
    </center>
  </xsl:template>

  <xsl:template match="pp.front">
    <center>
      <xsl:apply-templates/>
    </center>
  </xsl:template>

  <xsl:template match="title">
      <h1 class="title"><xsl:value-of select="."/></h1>
  </xsl:template>

  <xsl:template match="subtitle">
      <h2><xsl:value-of select="."/></h2>
  </xsl:template>

  <xsl:template match="pp.front.ccversion">
      <![CDATA[&nbsp;]]><br/>
      <b>Criteria Version: </b><xsl:apply-templates/><br/>
  </xsl:template>

  <xsl:template match="pp.front.label">
      <b>Protection Profile Label: </b><xsl:apply-templates/><br/>
  </xsl:template>

  <xsl:template match="pp.front.keywords">
      <b>Keywords: </b><xsl:apply-templates/><br/>
  </xsl:template>

  <xsl:template match="obname">
    <h3><xsl:value-of select="."/></h3>
  </xsl:template>

  <xsl:template match="obname" mode="funcprefix">
    <xsl:value-of select="@funcprefix"/>
  </xsl:template>

  <xsl:template match="draft">
    <em>D<![CDATA[&nbsp;]]>R<![CDATA[&nbsp;]]>A<![CDATA[&nbsp;]]>F<![CDATA[&nbsp;]]>T</em>
  </xsl:template>

  <xsl:template match="obgroup">
    <h2 class="obgroup"><xsl:value-of select="."/></h2>
  </xsl:template>

  <xsl:template match="operation">
    <tr valign="top">
      <td width="10%">OC = <xsl:value-of select="@value"/></td>
      <td colspan="2">
        <xsl:apply-templates select="opname"/> (<b>OC_<xsl:apply-templates select="preceding::obfuncprefix"/>_<xsl:value-of select="@name"/></b>)
        <xsl:apply-templates select="opdescrip"/>
      </td>
    </tr>
    <xsl:if test="count(descendant::arguments|descendant::returns|descendant::throws)!=0">
      <tr valign="top">
        <td><![CDATA[&nbsp;]]></td>
        <td colspan="1">
          <table>
            <tbody>
	      <xsl:if test="count(descendant::arguments)!=0">
		<xsl:variable name="whatpart">Arguments</xsl:variable>
		<xsl:apply-templates select="arguments"/>
	      </xsl:if>
	      <xsl:if test="count(descendant::returns)!=0">
		<xsl:variable name="whatpart">Returns</xsl:variable>
		<xsl:apply-templates select="returns"/>
	      </xsl:if>
	      <xsl:if test="count(descendant::throws)!=0">
		<xsl:variable name="whatpart">Exceptions</xsl:variable>
		<xsl:apply-templates select="throws"/>
	      </xsl:if>
            </tbody>
          </table>
        </td>
      </tr>
    </xsl:if>

    <tr valign="top">
      <td><![CDATA[&nbsp;]]></td>
      <td colspan="1">
        <table>
          <tbody>
            <tr valign="top">
              <td align="left">
                <tt>[C API]</tt>
              </td>
	      <td>
		<tt>result_t</tt>
	      </td>
	      <td>
		<tt><xsl:apply-templates select="preceding::obfuncprefix"/>_<xsl:apply-templates select="@name"/>(</tt>
	      </td>
	      <td>
		<tt>
		  <xsl:apply-templates select="preceding::obfuncprefix"/>_Key <em>this</em> /* in */
		  <xsl:for-each select="arguments/*|returns/*">
		    <xsl:text>,</xsl:text><br/>
		    <xsl:choose>
		      <xsl:when test='name()="key"'>
			<xsl:apply-templates select="@type"/>_Key
		      </xsl:when>
		      <xsl:when test='name()="word"'>
			uint32_t <xsl:if test='name(..)="returns"'>*</xsl:if>
		      </xsl:when>
		      <xsl:when test='name()="u64"'>
			uint64_t <xsl:if test='name(..)="returns"'>*</xsl:if>
		      </xsl:when>
		      <xsl:when test='name()="string"'>
			<xsl:text>uint32_t </xsl:text>
                        <xsl:if test='name(..)="returns"'>*</xsl:if>
                        <em><xsl:value-of select="@name"/>
                        <xsl:text>_len</xsl:text></em>
                        <xsl:choose>
                          <xsl:when test='name(..)="returns"'>
                            <xsl:text> /* in-out */</xsl:text>
                          </xsl:when>
                          <xsl:otherwise>
                            <xsl:text> /* in */</xsl:text>
                          </xsl:otherwise>
                        </xsl:choose>
                        <xsl:text>,</xsl:text><br/>
			<xsl:text>char *</xsl:text>
		      </xsl:when>
		      <xsl:otherwise>
			????
		      </xsl:otherwise>
		    </xsl:choose>
		    <em><xsl:value-of select="@name"/></em>
		    <xsl:choose>
                      <xsl:when test='name(..)="returns"'>
                        <xsl:text> /* out */</xsl:text>
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:text> /* in */</xsl:text>
                      </xsl:otherwise>
                    </xsl:choose>
		  </xsl:for-each>
		  <xsl:text> );</xsl:text>
		</tt>
	      </td>
            </tr>
            <tr valign="top">
              <td align="left">
                <tt>[C++ API]</tt>
              </td>
	      <td>
                <xsl:choose>
                  <xsl:when test="count(returns/word) = 0">
                    <tt>void</tt>
                  </xsl:when>
                  <xsl:otherwise>
		    <tt>uint32_t</tt>
                  </xsl:otherwise>
                </xsl:choose>
	      </td>
	      <td>
		<tt><xsl:apply-templates select="preceding::obfuncprefix"/>_<xsl:apply-templates select="@name"/>(</tt>
	      </td>
	      <td>
		<tt>
		  <xsl:apply-templates select="preceding::obfuncprefix"/>_Key <em>this</em> /* in */
		  <xsl:for-each select='arguments/*|returns/key|returns/string|returns/word[count(preceding-sibling::word)!=0]|returns/u64'>
		    <xsl:text>,</xsl:text><br/>
		    <xsl:choose>
		      <xsl:when test='name()="key"'>
			<xsl:apply-templates select="@type"/>_Key
		      </xsl:when>
		      <xsl:when test='name()="word"'>
			<xsl:text>uint32_t</xsl:text>
                        <xsl:if test='name(..)="returns"'>&amp;</xsl:if>
			<xsl:text> </xsl:text>
		      </xsl:when>
		      <xsl:when test='name()="u64"'>
			<xsl:text>uint64_t</xsl:text>
                        <xsl:if test='name(..)="returns"'>&amp;</xsl:if>
			<xsl:text> </xsl:text>
		      </xsl:when>
		      <xsl:when test='name()="string"'>
			<xsl:text>uint32_t</xsl:text>
                        <xsl:if test='name(..)="returns"'>&amp;</xsl:if>
			<xsl:text> </xsl:text>
                        <em><xsl:value-of select="@name"/>
                        <xsl:text>_len</xsl:text></em>
                        <xsl:choose>
                          <xsl:when test='name(..)="returns"'>
                            <xsl:text> /* in-out */</xsl:text>
                          </xsl:when>
                          <xsl:otherwise>
                            <xsl:text> /* in */</xsl:text>
                          </xsl:otherwise>
                        </xsl:choose>
                        <xsl:text>,</xsl:text><br/>
			<xsl:text>char *</xsl:text>
		      </xsl:when>
		      <xsl:otherwise>
			????
		      </xsl:otherwise>
		    </xsl:choose>
		    <em><xsl:value-of select="@name"/></em>
		    <xsl:choose>
                      <xsl:when test='name(..)="returns"'>
                        <xsl:text> /* out */</xsl:text>
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:text> /* in */</xsl:text>
                      </xsl:otherwise>
                    </xsl:choose>
		  </xsl:for-each>
		  <xsl:text> )</xsl:text>

                  <xsl:if test="count(throws) != 0">
                    <xsl:text> raises( </xsl:text>
		    <xsl:for-each select='throws'>
		      <xsl:choose>
			<xsl:when test="position() &gt; 1">
			  <xsl:text>, </xsl:text>
			</xsl:when>
		      </xsl:choose>
		      <u><xsl:value-of select="@value"/></u>
		    </xsl:for-each>
		    <xsl:text> )</xsl:text>
                  </xsl:if>

		  <xsl:text>;</xsl:text>
		</tt>
	      </td>
            </tr>
            <tr valign="top">
              <td align="left">
                <tt>[Transport]</tt>
              </td>
	      <td>
                Sent
	      </td>
	      <td>
                 r0
              </td>
              <td>
                <tt>OC_<xsl:apply-templates select="preceding::obfuncprefix"/>_<xsl:value-of select="@name"/></tt> (<xsl:value-of select="@value"/>)
              </td>
            </tr>
	    <xsl:for-each select="arguments/word[count(preceding-sibling::word) &lt; 3]">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
	          r<xsl:number value="count(preceding-sibling::word)+1"/>
                </td>
                <td>
	          <em><xsl:value-of select="@name"/></em>
                </td>
	      </tr>
	    </xsl:for-each>
	    <xsl:for-each select="arguments/key">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
	          k<xsl:number value="count(preceding-sibling::key)"/>
                </td>
                <td>
	          <em><xsl:value-of select="@name"/></em>
                </td>
	      </tr>
	    </xsl:for-each>
            <!-- The xsl:variable mechanism really isn't powerful enough to
                 deal with alignment issues, because things defined with 
                 xsl:variable cannot be modified once defined. These means 
                 that you cannot compute an "accumulated offset" or perform
                 rounding on the offset.

                 We work around this by pre-sorting the blobs in the transmit 
                 string according to their size. As it happens, this gets us
                 a valid alignment. This mechanism will break down the first 
                 time somebody needs to specify a structure argument. At that 
                 point we will either need to introduce an xslt extension or
                 cave in and actually write CapIDL. -->
	    <xsl:for-each select="arguments/u64">
	      <tr valign="top">
		<td><![CDATA[&nbsp;]]></td>
		<td><![CDATA[&nbsp;]]></td>
		<td>
		  <xsl:variable name="mypos"><xsl:number value="(position()-1)*8"/></xsl:variable>
		  str 
                  <xsl:text>[</xsl:text>
                  <xsl:number value="$mypos"/>
                  <xsl:text>:</xsl:text>
                  <xsl:number value="$mypos+7"/>
                  <xsl:text>]</xsl:text>
		</td>
		<td>
		  <em><xsl:value-of select="@name"/></em>
		</td>
	      </tr>
	    </xsl:for-each>
	    <xsl:for-each select="arguments/word[count(preceding-sibling::word) &gt;= 3]">
	      <tr valign="top">
		<td><![CDATA[&nbsp;]]></td>
		<td><![CDATA[&nbsp;]]></td>
		<td>
		  <xsl:variable name="mypos"><xsl:number value="count(../u64)*8+(position()-1)*4"/></xsl:variable>
		  str 
                  <xsl:text>[</xsl:text>
                  <xsl:value-of select="$mypos"/>
                  <xsl:text>:</xsl:text>
                  <xsl:number value="$mypos+3"/>
                  <xsl:text>]</xsl:text>
		</td>
		<td>
		  <em><xsl:value-of select="@name"/></em>
		</td>
	      </tr>
	    </xsl:for-each>
	    <xsl:for-each select="arguments/string">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
		  <xsl:variable name="mypos"><xsl:number value="count(../u64)*8+count(../word[count(preceding-sibling::word) &gt;= 3])*4"/></xsl:variable>
	          str
	          <xsl:if test="count(../word[4]) != 0">
		    <xsl:text>[</xsl:text>
                    <xsl:number value="$mypos"/>
		    <xsl:text>...]</xsl:text>
                  </xsl:if>
                </td>
                <td>
	          <em><xsl:value-of select="@name"/></em>
                </td>
	      </tr>
	    </xsl:for-each>
	    <xsl:if test="count(arguments/string|arguments/word[4]|arguments/u64) != 0">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
	          len
                </td>
                <td>
                  <xsl:if test="count(arguments/string) != 0">
	            <em><xsl:value-of select="arguments/string/@name"/>
                        <xsl:text>_len</xsl:text></em>
                    <xsl:if test="count(arguments/word[4]|arguments/u64) != 0">
		      <xsl:text> + </xsl:text>
                    </xsl:if>
                  </xsl:if>
                  <xsl:if test="count(arguments/word[4]|arguments/u64) != 0">
                    <xsl:number value="count(arguments/word[count(preceding-sibling::word) &gt;= 3])*4"/>
                  </xsl:if>
                </td>
	      </tr>
	    </xsl:if>
            <tr valign="top">
              <td align="left">
                <![CDATA[&nbsp;]]>
              </td>
	      <td>
                Received
	      </td>
	      <td>
                 r0
              </td>
              <td>
                (result or exception value)
              </td>
            </tr>
	    <xsl:for-each select="returns/word[count(preceding-sibling::word) &lt; 3]">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
	          r<xsl:number value="count(preceding-sibling::word)+1"/>
                </td>
                <td>
	          <em><xsl:value-of select="@name"/></em>
                </td>
	      </tr>
	    </xsl:for-each>
	    <xsl:for-each select="returns/key">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
	          k<xsl:number value="count(preceding-sibling::key)"/>
                </td>
                <td>
	          <em><xsl:value-of select="@name"/></em>
                </td>
	      </tr>
	    </xsl:for-each>
            <!-- The xsl:variable mechanism really isn't powerful enough to
                 deal with alignment issues, because things defined with 
                 xsl:variable cannot be modified once defined. These means 
                 that you cannot compute an "accumulated offset" or perform
                 rounding on the offset.

                 We work around this by pre-sorting the blobs in the transmit 
                 string according to their size. As it happens, this gets us
                 a valid alignment. This mechanism will break down the first 
                 time somebody needs to specify a structure argument. At that 
                 point we will either need to introduce an xslt extension or
                 cave in and actually write CapIDL. -->
	    <xsl:for-each select="returns/u64">
	      <tr valign="top">
		<td><![CDATA[&nbsp;]]></td>
		<td><![CDATA[&nbsp;]]></td>
		<td>
		  <xsl:variable name="mypos"><xsl:number value="(position()-1)*8"/></xsl:variable>
		  str 
                  <xsl:text>[</xsl:text>
                  <xsl:number value="$mypos"/>
                  <xsl:text>:</xsl:text>
                  <xsl:number value="$mypos+7"/>
                  <xsl:text>]</xsl:text>
		</td>
		<td>
		  <em><xsl:value-of select="@name"/></em>
		</td>
	      </tr>
	    </xsl:for-each>
	    <xsl:for-each select="returns/word[count(preceding-sibling::word) &gt;= 3]">
	      <tr valign="top">
		<td><![CDATA[&nbsp;]]></td>
		<td><![CDATA[&nbsp;]]></td>
		<td>
		  <xsl:variable name="mypos"><xsl:number value="count(../u64)*8+(position()-1)*4"/></xsl:variable>
		  str 
                  <xsl:text>[</xsl:text>
                  <xsl:value-of select="$mypos"/>
                  <xsl:text>:</xsl:text>
                  <xsl:number value="$mypos+3"/>
                  <xsl:text>]</xsl:text>
		</td>
		<td>
		  <em><xsl:value-of select="@name"/></em>
		</td>
	      </tr>
	    </xsl:for-each>
	    <xsl:for-each select="returns/string">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
		  <xsl:variable name="mypos"><xsl:number value="count(../u64)*8+count(../word[count(preceding-sibling::word) &gt;= 3])*4"/></xsl:variable>
	          str
	          <xsl:if test="count(../word[4]) != 0">
		    <xsl:text>[</xsl:text>
                    <xsl:number value="$mypos"/>
		    <xsl:text>...]</xsl:text>
                  </xsl:if>
                </td>
                <td>
	          <em><xsl:value-of select="@name"/></em>
                </td>
	      </tr>
	    </xsl:for-each>
	    <xsl:if test="count(returns/string|returns/word[4]|returns/u64) != 0">
	      <tr valign="top">
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td>
	          len
                </td>
                <td>
                  <xsl:if test="count(returns/string) != 0">
	            <em><xsl:value-of select="returns/string/@name"/>
                        <xsl:text>_len</xsl:text></em>
                    <xsl:if test="count(returns/word[4]|returns/u64) != 0">
		      <xsl:text> + </xsl:text>
                    </xsl:if>
                  </xsl:if>
                  <xsl:if test="count(returns/word[4]|returns/u64) != 0">
                    <xsl:number value="count(returns/word[count(preceding-sibling::word) &gt;= 3])*4"/>
                  </xsl:if>
                </td>
	      </tr>
	    </xsl:if>
            <tr valign="top">
              <td align="left">
                <tt>[C Stub]</tt>
              </td>
	      <td>
                #include &lt;target.h&gt;
	      </td>
            </tr>	
            <tr valign="top">
              <td align="left">
	         <![CDATA[&nbsp;]]>
              </td>
	      <td>
		<tt>result_t</tt>
	      </td>
            </tr>
            <tr valign="top">
	      <td>
                <![CDATA[&nbsp;]]>
	      </td>
	      <td>
		<tt><xsl:apply-templates select="preceding::obfuncprefix"/>_<xsl:apply-templates select="@name"/>(</tt>
	      </td>
	      <td colspan="2">
		<tt>
		  <xsl:apply-templates select="preceding::obfuncprefix"/>_Key <em>this</em> /* in */
		  <xsl:for-each select="arguments/*|returns/*">
		    <xsl:text>,</xsl:text><br/>
		    <xsl:choose>
		      <xsl:when test='name()="key"'>
			<xsl:apply-templates select="@type"/>_Key
		      </xsl:when>
		      <xsl:when test='name()="word"'>
			uint32_t <xsl:if test='name(..)="returns"'>*</xsl:if>
		      </xsl:when>
		      <xsl:when test='name()="u64"'>
			uint64_t <xsl:if test='name(..)="returns"'>*</xsl:if>
		      </xsl:when>
		      <xsl:when test='name()="string"'>
			<xsl:text>uint32_t </xsl:text>
                        <xsl:if test='name(..)="returns"'>*</xsl:if>
                        <em><xsl:value-of select="@name"/>
                        <xsl:text>_len</xsl:text></em>
                        <xsl:choose>
                          <xsl:when test='name(..)="returns"'>
                            <xsl:text> /* in-out */</xsl:text>
                          </xsl:when>
                          <xsl:otherwise>
                            <xsl:text> /* in */</xsl:text>
                          </xsl:otherwise>
                        </xsl:choose>
                        <xsl:text>,</xsl:text><br/>
			<xsl:text>char *</xsl:text>
		      </xsl:when>
		      <xsl:otherwise>
			????
		      </xsl:otherwise>
		    </xsl:choose>
		    <em><xsl:value-of select="@name"/></em>
		    <xsl:choose>
                      <xsl:when test='name(..)="returns"'>
                        <xsl:text> /* out */</xsl:text>
                      </xsl:when>
                      <xsl:otherwise>
                        <xsl:text> /* in */</xsl:text>
                      </xsl:otherwise>
                    </xsl:choose>
		  </xsl:for-each>
		  <xsl:text> )</xsl:text>
		</tt>
	      </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td>{</td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">result_t result;</td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">Message msg;</td>
            </tr>
	    <xsl:if test="count(arguments/u64|arguments/word[count(preceding-sibling::word) &gt;= 3]) > 0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">struct sndblock {</td>
              </tr>
	      <xsl:for-each select="arguments/u64">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>uint64_t <xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="arguments/word[count(preceding-sibling::word) &gt;= 3]">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>uint32_t <xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="arguments/string">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>char <xsl:value-of select="@name"/>[0];</td>
                </tr>
              </xsl:for-each>
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">} ;</td>
              </tr>
	      <xsl:if test="count(arguments/string) = 0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">struct snddblock theSndBlock;</td>
              </tr>
              </xsl:if>
            </xsl:if>
	    <xsl:if test="count(returns/u64|returns/word[count(preceding-sibling::word) &gt;= 3]) > 0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">struct rcvblock {</td>
              </tr>
	      <xsl:for-each select="returns/u64">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>uint64_t <xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="returns/word[count(preceding-sibling::word) &gt;= 3]">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>uint32_t <xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="returns/string">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>char <xsl:value-of select="@name"/>[0];</td>
                </tr>
              </xsl:for-each>
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">} ;</td>
              </tr>
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">struct sndblock *psndblk;</td>
              </tr>
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">struct rcvblock *prcvblk;</td>
              </tr>
	      <xsl:if test="count(returns/string) = 0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">struct rcvblock theRcvBlock;</td>
              </tr>
              </xsl:if>
            </xsl:if>
            <xsl:choose>
	      <xsl:when test="count(arguments/u64|arguments/word[count(preceding-sibling::word) &gt;= 3]) > 0 and count(arguments/string) > 0">
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_len = sizeof(struct sndblock) + <xsl:value-of select="arguments/string/@name"/>_len;</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">psndblk = alloca(msg.snd_len);</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_data = psndblk;</td>
		</tr>
	      </xsl:when>
	      <xsl:when test="count(arguments/u64|arguments/word[count(preceding-sibling::word) &gt;= 3]) > 0 and count(arguments/string) = 0">
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_len = sizeof(struct sndblock);</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">psndblk = &amp;theSndBlock;</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_data = psndblk;</td>
		</tr>
	      </xsl:when>
	      <xsl:when test="count(arguments/u64|arguments/word[count(preceding-sibling::word) &gt;= 3]) = 0 and count(arguments/string) > 0">
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_len = <xsl:value-of select="arguments/string/@name"/>_len;</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_data = <xsl:value-of select="arguments/string/@name"/>;</td>
		</tr>
	      </xsl:when>
              <xsl:otherwise>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.snd_len = 0;</td>
		</tr>
              </xsl:otherwise>
            </xsl:choose>

            <xsl:choose>
	      <xsl:when test="count(returns/u64|returns/word[count(preceding-sibling::word) &gt;= 3]) > 0 and count(returns/string) > 0">
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_len = sizeof(struct rcvblock) + <xsl:value-of select="returns/string/@name"/>_len;</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">prcvblk = alloca(msg.rcv_len);</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_data = prcvblk;</td>
		</tr>
	      </xsl:when>
	      <xsl:when test="count(returns/u64|returns/word[count(preceding-sibling::word) &gt;= 3]) > 0 and count(returns/string) = 0">
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_len = sizeof(struct sndblock);</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">prcvblk = &amp;theRcvBlock;</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_data = prcvblk;</td>
		</tr>
	      </xsl:when>
	      <xsl:when test="count(returns/u64|returns/word[count(preceding-sibling::word) &gt;= 3]) = 0 and count(returns/string) > 0">
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_len = <xsl:value-of select="returns/string/@name"/>_len;</td>
		</tr>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_data = <xsl:value-of select="returns/string/@name"/>;</td>
		</tr>
	      </xsl:when>
              <xsl:otherwise>
		<tr>
		  <td><![CDATA[&nbsp;]]></td>
		  <td><![CDATA[&nbsp;]]></td>
		  <td colspan="2">msg.rcv_len = 0;</td>
		</tr>
              </xsl:otherwise>
            </xsl:choose>

	    <xsl:if test="count(arguments/u64|arguments/word[count(preceding-sibling::word) &gt;= 3]) > 0">
	      <xsl:for-each select="arguments/u64">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">psndblock-&gt;<xsl:value-of select="@name"/> = <xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="arguments/word[count(preceding-sibling::word) &gt;= 3]">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">psndblock-&gt;<xsl:value-of select="@name"/> = <xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="arguments/string">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">bcopy(&amp;psndblock-&gt;<xsl:value-of select="@name"/>, <xsl:value-of select="@name"/>, <xsl:value-of select="@name"/>_len);</td>
                </tr>
              </xsl:for-each>
            </xsl:if>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">msg.inv_key = <em>this</em>;</td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">msg.snd_w0 = <b>OC_<xsl:apply-templates select="preceding::obfuncprefix"/>_<xsl:value-of select="@name"/></b>;</td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/word[1])!=0">
                    msg.snd_w1 = <xsl:value-of select="arguments/word[1]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_w1 = 0;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/word[2])!=0">
                    msg.snd_w2 = <xsl:value-of select="arguments/word[2]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_w2 = 0;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/word[3])!=0">
                    msg.snd_w3 = <xsl:value-of select="arguments/word[3]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_w3 = 0;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/key[1])!=0">
                    msg.snd_key0 = <xsl:value-of select="arguments/key[1]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_key0 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/key[2])!=0">
                    msg.snd_key1 = <xsl:value-of select="arguments/key[2]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_key1 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/key[3])!=0">
                    msg.snd_key2 = <xsl:value-of select="arguments/key[3]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_key2 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(arguments/key[4])!=0">
                    msg.snd_key3 = <xsl:value-of select="arguments/key[4]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.snd_key3 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(returns/key[1])!=0">
                    msg.rcv_key0 = <xsl:value-of select="returns/key[1]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.rcv_key0 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(returns/key[2])!=0">
                    msg.rcv_key1 = <xsl:value-of select="returns/key[2]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.rcv_key1 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(returns/key[3])!=0">
                    msg.rcv_key2 = <xsl:value-of select="returns/key[3]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.rcv_key2 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                <xsl:choose>
                  <xsl:when test="count(returns/key[4])!=0">
                    msg.rcv_key3 = <xsl:value-of select="returns/key[4]/@name"/>;
                  </xsl:when>
                  <xsl:otherwise>
                    msg.rcv_key3 = <b>KR_VOID</b>;
                  </xsl:otherwise>
                </xsl:choose>
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                result = CALL(&amp;msg);
              </td>
            </tr>
	    <xsl:if test="count(returns/u64|returns/word[count(preceding-sibling::word) &gt;= 3]) > 0">
	      <xsl:for-each select="returns/u64">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">if (<xsl:value-of select="@name"/>) *<xsl:value-of select="@name"/> = prcvblock-&gt;<xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="returns/word[count(preceding-sibling::word) &gt;= 3]">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">if (<xsl:value-of select="@name"/>) *<xsl:value-of select="@name"/> = prcvblock-&gt;<xsl:value-of select="@name"/>;</td>
                </tr>
              </xsl:for-each>
	      <xsl:for-each select="returns/string">
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">{</td>
                </tr>
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>rcv_len = msg.rcv_len - sizeof(struct rcvblk));</td>
                </tr>
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>if (<xsl:value-of select="@name"/>_len) *<xsl:value-of select="@name"/>_len = rcv_len;</td>
                </tr>
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2"><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]><![CDATA[&nbsp;]]>bcopy(<xsl:value-of select="@name"/>, &amp;prcvblock-&gt;<xsl:value-of select="@name"/>, rcv_len);</td>
                </tr>
	        <tr>
                  <td><![CDATA[&nbsp;]]></td>
                  <td><![CDATA[&nbsp;]]></td>
                  <td colspan="2">}</td>
                </tr>
              </xsl:for-each>
            </xsl:if>
            <xsl:if test="count(returns/word[1])!=0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">
                  if (<xsl:value-of select="returns/word[1]/@name"/>) *<xsl:value-of select="returns/word[1]/@name"/> = msg.rcv_w1;
                </td>
              </tr>
            </xsl:if>
            <xsl:if test="count(returns/word[2])!=0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">
                  if (<xsl:value-of select="returns/word[2]/@name"/>) *<xsl:value-of select="returns/word[2]/@name"/> = msg.rcv_w2;
                </td>
              </tr>
            </xsl:if>
            <xsl:if test="count(returns/word[3])!=0">
	      <tr>
                <td><![CDATA[&nbsp;]]></td>
                <td><![CDATA[&nbsp;]]></td>
                <td colspan="2">
                  if (<xsl:value-of select="returns/word[3]/@name"/>) *<xsl:value-of select="returns/word[3]/@name"/> = msg.rcv_w3;
                </td>
              </tr>
            </xsl:if>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td><![CDATA[&nbsp;]]></td>
              <td colspan="2">
                return result;
              </td>
            </tr>
	    <tr>
              <td><![CDATA[&nbsp;]]></td>
              <td>}</td>
            </tr>
          </tbody>
        </table>
      </td>
    </tr>
  </xsl:template>

  <xsl:template match="opname">
    <em><xsl:apply-templates/></em>
  </xsl:template>

  <xsl:template match="key|word|string">
    <tr valign="top">
      <xsl:choose>
        <xsl:when test="count(preceding-sibling::*)=0">
          <td align="left"><b><xsl:value-of select="$whatpart"/></b></td>
        </xsl:when>
	<xsl:otherwise>
          <td align="left"><![CDATA[&nbsp;]]></td>
	</xsl:otherwise>
      </xsl:choose>
      <td align="left">
        <em>
          <xsl:value-of select="@name"/>
          <xsl:if test='name()="string"'>
           <xsl:text>, </xsl:text>
           <xsl:value-of select="@name"/>
           <xsl:text>_len</xsl:text>
          </xsl:if>
        </em>
      </td>
      <td><xsl:apply-templates/></td>
    </tr>
  </xsl:template>

  <xsl:template match="throws">
    <tr valign="top">
      <xsl:if test="count(preceding-sibling::throws)!=0">
        <td><![CDATA[&nbsp;]]></td>
      </xsl:if>
      <xsl:if test="count(preceding-sibling::throws)=0">
        <td><b><xsl:value-of select="$whatpart"/></b></td>
      </xsl:if>
      <td><em><xsl:value-of select="@value"/></em></td>
      <td><xsl:apply-templates/></td>
    </tr>
  </xsl:template>

  <xsl:template match="keytype">
    <tr valign="top">
      <td><b><xsl:value-of select="@value"/></b></td>
      <td><xsl:apply-templates/></td>
    </tr>
  </xsl:template>

  <xsl:template match="author/name">
    <em><xsl:apply-templates/></em><br/>
  </xsl:template>

  <xsl:template match="author/organization">
    <xsl:apply-templates/><br/>
  </xsl:template>

  <xsl:template match="author/address">
    <xsl:apply-templates/><br/>
  </xsl:template>

  <xsl:template match="author/email">
    <tt><xsl:apply-templates/></tt><br/>
  </xsl:template>

  <xsl:template match="abstract">
    <table width="100%"><tbody><tr valign="top"><td
    width="5%"><![CDATA[&nbsp;]]></td><td width="90%"><div><![CDATA[&nbsp;]]><br/></div><xsl:apply-templates mode="abstract"/></td><td width="5%"><![CDATA[&nbsp;]]></td></tr></tbody></table>
  </xsl:template>

  <xsl:template match="pp.disclaimer">
    <table width="100%"><tbody><tr valign="top"><td
    width="5%"><![CDATA[&nbsp;]]></td><td width="90%"><div><![CDATA[&nbsp;]]><br/></div><b>Disclaimer</b><br/><xsl:apply-templates mode="abstract"/></td><td width="5%"><![CDATA[&nbsp;]]></td></tr></tbody></table>
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
</xsl:stylesheet>
		
