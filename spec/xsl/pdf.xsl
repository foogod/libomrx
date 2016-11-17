<?xml version='1.0'?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:fo="http://www.w3.org/1999/XSL/Format"
                xmlns:fox="http://xmlgraphics.apache.org/fop/extensions"
                version="1.0">
  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/fo/docbook.xsl"/>
  <xsl:include href="generated/titlepage.templates.xsl"/>

  <xsl:param name="body.font.family">Droid Serif,serif</xsl:param>

  <xsl:param name="section.autolabel">1</xsl:param>
  <xsl:param name="section.label.includes.component.label">1</xsl:param>
  <xsl:param name="section.autolabel.max.depth">2</xsl:param>

  <xsl:param name="draft.watermark.image">images/draft.png</xsl:param>
  <xsl:param name="admon.graphics">1</xsl:param>
  <xsl:param name="admon.graphics.extension">.svg</xsl:param>
  <xsl:attribute-set name="graphical.admonition.properties">
      <xsl:attribute name="margin-left">-54pt</xsl:attribute>
      <xsl:attribute name="margin-left"><xsl:value-of select="concat('-', $graphic.width)"/></xsl:attribute>
  </xsl:attribute-set>
  <xsl:attribute-set name="admonition.title.properties">
    <xsl:attribute name="font-size">12pt</xsl:attribute>
    <xsl:attribute name="space-after.minimum">2pt</xsl:attribute>
    <xsl:attribute name="space-after.optimum">2pt</xsl:attribute>
    <xsl:attribute name="space-after.maximum">2pt</xsl:attribute>
  </xsl:attribute-set>

  <!-- Custom styling for various element types -->

  <!-- confterm style is used for "conformance terms" ("must", etc) -->
  <xsl:attribute-set name="confterm.properties">
    <xsl:attribute name="font-weight">bold</xsl:attribute>
  </xsl:attribute-set>
  <!-- <literal> is used to indicate literal byte sequences in a datastream -->
  <xsl:attribute-set name="literal.properties">
    <xsl:attribute name="font-family">monospace</xsl:attribute>
    <xsl:attribute name="font-size">0.9em</xsl:attribute>
    <xsl:attribute name="border-style">solid</xsl:attribute>
    <xsl:attribute name="border-width">0.1mm</xsl:attribute>
    <xsl:attribute name="fox:border-radius">0.2mm</xsl:attribute>
    <xsl:attribute name="padding-top">0.3mm</xsl:attribute>
  </xsl:attribute-set>
  <!-- <type> is used for elemental datatype names -->
  <xsl:attribute-set name="type.properties">
    <xsl:attribute name="font-family">Droid Serif,serif</xsl:attribute>
    <xsl:attribute name="font-style">italic</xsl:attribute>
  </xsl:attribute-set>
  <!-- <structfield> is used for fields within header definitions, etc -->
  <xsl:attribute-set name="structfield.properties">
    <xsl:attribute name="font-family">Crete Round,symbol</xsl:attribute>
    <xsl:attribute name="font-style">italic</xsl:attribute>
  </xsl:attribute-set>
  <!-- <property> is used for chunk attribute names -->
  <xsl:attribute-set name="property.properties">
    <xsl:attribute name="font-family">Crete Round,symbol</xsl:attribute>
    <xsl:attribute name="font-style">italic</xsl:attribute>
  </xsl:attribute-set>
  <!-- <classname> is used for chunk tags -->
  <xsl:attribute-set name="classname.properties">
    <xsl:attribute name="font-family">Crete Round,symbol</xsl:attribute>
  </xsl:attribute-set>
  <!-- todo/fixme styles are used for &todo; and &fixme; editing notations -->
  <xsl:attribute-set name="todo.properties">
    <xsl:attribute name="font-weight">bold</xsl:attribute>
    <xsl:attribute name="border-style">solid</xsl:attribute>
    <xsl:attribute name="border-width">0.2mm</xsl:attribute>
    <xsl:attribute name="background-color">blue</xsl:attribute>
    <xsl:attribute name="color">white</xsl:attribute>
    <xsl:attribute name="padding-top">0.2em</xsl:attribute>
    <xsl:attribute name="padding-start">0.2em</xsl:attribute>
    <xsl:attribute name="padding-end">0.2em</xsl:attribute>
    <xsl:attribute name="padding-bottom">0em</xsl:attribute>
  </xsl:attribute-set>
  <xsl:attribute-set name="fixme.properties" use-attribute-sets="todo.properties">
    <xsl:attribute name="background-color">red</xsl:attribute>
  </xsl:attribute-set>
  <!-- Make links show up dark blue -->
  <xsl:attribute-set name="xref.properties">
    <xsl:attribute name="color">navy</xsl:attribute>
  </xsl:attribute-set>
  <!-- Make all text within tables slightly smaller by default (helps everything fit in the cells and generally looks better) -->
  <xsl:attribute-set name="table.properties">
    <xsl:attribute name="font-size">0.9em</xsl:attribute>
  </xsl:attribute-set>
  <xsl:attribute-set name="informaltable.properties">
    <xsl:attribute name="font-size">0.9em</xsl:attribute>
  </xsl:attribute-set>

  <!-- templates to apply above stylings -->

  <xsl:template match="emphasis[@role='conformance-term']">
    <fo:inline xsl:use-attribute-sets="confterm.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="literal">
    <fo:inline xsl:use-attribute-sets="literal.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="type">
    <fo:inline xsl:use-attribute-sets="type.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="structfield">
    <fo:inline xsl:use-attribute-sets="structfield.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="property">
    <fo:inline xsl:use-attribute-sets="property.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="classname">
    <fo:inline xsl:use-attribute-sets="classname.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="emphasis[@role='fixme']">
    <fo:inline xsl:use-attribute-sets="fixme.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>

  <xsl:template match="emphasis[@role='todo']">
    <fo:inline xsl:use-attribute-sets="todo.properties">
      <xsl:apply-templates/>
    </fo:inline>
  </xsl:template>


  <!-- This overly-complicated hack is required in order to get tables to be
       rendered centered horizontally within the text column. -->
  <xsl:template name="table.layout">
    <xsl:param name="table.content"/>

    <fo:table width="100%" table-layout="fixed">
      <fo:table-column column-width="proportional-column-width(1)"/>
      <fo:table-column column-width="90%"/>
      <fo:table-column column-width="proportional-column-width(1)"/>
      <fo:table-body start-indent="0pt">
        <fo:table-row>
          <fo:table-cell><fo:block/></fo:table-cell>
          <fo:table-cell>

            <xsl:copy-of select="$table.content"/>

          </fo:table-cell>
          <fo:table-cell><fo:block/></fo:table-cell>
        </fo:table-row>
      </fo:table-body>
    </fo:table>
  </xsl:template>

</xsl:stylesheet>
