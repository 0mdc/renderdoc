/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2018 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "PipelineStateViewer.h"
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QSvgRenderer>
#include <QToolButton>
#include <QXmlStreamWriter>
#include "3rdparty/toolwindowmanager/ToolWindowManager.h"
#include "Code/QRDUtils.h"
#include "Code/Resources.h"
#include "Widgets/Extended/RDLabel.h"
#include "D3D11PipelineStateViewer.h"
#include "D3D12PipelineStateViewer.h"
#include "GLPipelineStateViewer.h"
#include "VulkanPipelineStateViewer.h"
#include "ui_PipelineStateViewer.h"

PipelineStateViewer::PipelineStateViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::PipelineStateViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;

  for(size_t i = 0; i < ARRAY_COUNT(editMenus); i++)
    editMenus[i] = new QMenu(this);

  setToD3D11();

  m_Ctx.AddCaptureViewer(this);
}

PipelineStateViewer::~PipelineStateViewer()
{
  reset();

  m_Ctx.BuiltinWindowClosed(this);
  m_Ctx.RemoveCaptureViewer(this);

  delete ui;
}

void PipelineStateViewer::OnCaptureLoaded()
{
  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D11)
    setToD3D11();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
    setToD3D12();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    setToGL();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
    setToVulkan();

  if(m_Current)
    m_Current->OnCaptureLoaded();
}

void PipelineStateViewer::OnCaptureClosed()
{
  if(m_Current)
    m_Current->OnCaptureClosed();
}

void PipelineStateViewer::OnEventChanged(uint32_t eventId)
{
  if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D11)
    setToD3D11();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::D3D12)
    setToD3D12();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::OpenGL)
    setToGL();
  else if(m_Ctx.APIProps().pipelineType == GraphicsAPI::Vulkan)
    setToVulkan();

  if(m_Current)
    m_Current->OnEventChanged(eventId);
}

QString PipelineStateViewer::GetCurrentAPI()
{
  if(m_Current == m_D3D11)
    return lit("D3D11");
  else if(m_Current == m_D3D12)
    return lit("D3D12");
  else if(m_Current == m_GL)
    return lit("OpenGL");
  else if(m_Current == m_Vulkan)
    return lit("Vulkan");

  return lit("");
}

QVariant PipelineStateViewer::persistData()
{
  QVariantMap state;

  state[lit("type")] = GetCurrentAPI();

  return state;
}

void PipelineStateViewer::setPersistData(const QVariant &persistData)
{
  QString str = persistData.toMap()[lit("type")].toString();

  if(str == lit("D3D11"))
    setToD3D11();
  else if(str == lit("D3D12"))
    setToD3D12();
  else if(str == lit("GL"))
    setToGL();
  else if(str == lit("Vulkan"))
    setToVulkan();
}

void PipelineStateViewer::reset()
{
  delete m_D3D11;
  delete m_D3D12;
  delete m_GL;
  delete m_Vulkan;

  m_D3D11 = NULL;
  m_D3D12 = NULL;
  m_GL = NULL;
  m_Vulkan = NULL;

  m_Current = NULL;
}

void PipelineStateViewer::setToD3D11()
{
  if(m_D3D11)
    return;

  reset();

  m_D3D11 = new D3D11PipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_D3D11);
  m_Current = m_D3D11;
}

void PipelineStateViewer::setToD3D12()
{
  if(m_D3D12)
    return;

  reset();

  m_D3D12 = new D3D12PipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_D3D12);
  m_Current = m_D3D12;
}

void PipelineStateViewer::setToGL()
{
  if(m_GL)
    return;

  reset();

  m_GL = new GLPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_GL);
  m_Current = m_GL;
}

void PipelineStateViewer::setToVulkan()
{
  if(m_Vulkan)
    return;

  reset();

  m_Vulkan = new VulkanPipelineStateViewer(m_Ctx, *this, this);
  ui->layout->addWidget(m_Vulkan);
  m_Current = m_Vulkan;
}

QXmlStreamWriter *PipelineStateViewer::beginHTMLExport()
{
  if(!m_Ctx.IsCaptureLoaded())
    return NULL;

  QString filename = RDDialog::getSaveFileName(this, tr("Export pipeline state as HTML"), QString(),
                                               tr("HTML files (*.html)"));

  if(!filename.isEmpty())
  {
    ANALYTIC_SET(Export.PipelineState, true);

    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile *f = new QFile(filename, this);
      if(f->open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        QXmlStreamWriter *xmlptr = new QXmlStreamWriter(f);

        QXmlStreamWriter &xml = *xmlptr;

        xml.setAutoFormatting(true);
        xml.setAutoFormattingIndent(4);
        xml.writeStartDocument();
        xml.writeDTD(lit("<!DOCTYPE html>"));

        xml.writeStartElement(lit("html"));
        xml.writeAttribute(lit("lang"), lit("en"));

        QString title = tr("%1 EID %2 - %3 Pipeline export")
                            .arg(QFileInfo(m_Ctx.GetCaptureFilename()).fileName())
                            .arg(m_Ctx.CurEvent())
                            .arg(GetCurrentAPI());

        {
          xml.writeStartElement(lit("head"));

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("charset"), lit("utf-8"));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("http-equiv"), lit("X-UA-Compatible"));
          xml.writeAttribute(lit("content"), lit("IE=edge"));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("name"), lit("viewport"));
          xml.writeAttribute(lit("content"), lit("width=device-width, initial-scale=1"));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("name"), lit("description"));
          xml.writeAttribute(lit("content"), lit(""));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("name"), lit("author"));
          xml.writeAttribute(lit("content"), lit(""));
          xml.writeEndElement();

          xml.writeStartElement(lit("meta"));
          xml.writeAttribute(lit("http-equiv"), lit("Content-Type"));
          xml.writeAttribute(lit("content"), lit("text/html;charset=utf-8"));
          xml.writeEndElement();

          xml.writeStartElement(lit("title"));
          xml.writeCharacters(title);
          xml.writeEndElement();

          xml.writeStartElement(lit("style"));
          xml.writeComment(lit(R"(

/* If you think this css is ugly/bad, open a pull request! */
body { margin: 20px; }
div.stage { border: 1px solid #BBBBBB; border-radius: 5px; padding: 16px; margin-bottom: 32px; }
div.stage h1 { text-decoration: underline; margin-top: 0px; }
div.stage table { border: 1px solid #AAAAAA; border-collapse: collapse; }
div.stage table thead tr { border-bottom: 1px solid #AAAAAA; background-color: #EEEEFF; }
div.stage table tr th { border-right: 1px solid #AAAAAA; padding: 6px; }
div.stage table tr td { border-right: 1px solid #AAAAAA; background-color: #EEEEEE; padding: 3px; }

)"));
          xml.writeEndElement();    // </style>

          xml.writeEndElement();    // </head>
        }

        {
          xml.writeStartElement(lit("body"));

          xml.writeStartElement(lit("h1"));
          xml.writeCharacters(title);
          xml.writeEndElement();

          xml.writeStartElement(lit("h3"));
          {
            QString context = tr("Frame %1").arg(m_Ctx.FrameInfo().frameNumber);

            const DrawcallDescription *draw = m_Ctx.CurDrawcall();

            QList<const DrawcallDescription *> drawstack;
            const DrawcallDescription *parent = draw->parent;
            while(parent)
            {
              drawstack.push_front(parent);
              parent = parent->parent;
            }

            for(const DrawcallDescription *d : drawstack)
            {
              context += QFormatStr(" > %1").arg(d->name);
            }

            context += QFormatStr(" => %1").arg(draw->name);

            xml.writeCharacters(context);
          }
          xml.writeEndElement();    // </h3>
        }

        // body is open

        return xmlptr;
      }

      RDDialog::critical(
          this, tr("Error exporting pipeline state"),
          tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f->errorString()));

      delete f;

      return NULL;
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
      return NULL;
    }
  }

  return NULL;
}

void PipelineStateViewer::exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols,
                                          const QList<QVariantList> &rows)
{
  xml.writeStartElement(lit("table"));

  {
    xml.writeStartElement(lit("thead"));
    xml.writeStartElement(lit("tr"));

    for(const QString &col : cols)
    {
      xml.writeStartElement(lit("th"));
      xml.writeCharacters(col);
      xml.writeEndElement();
    }

    xml.writeEndElement();
    xml.writeEndElement();
  }

  {
    xml.writeStartElement(lit("tbody"));

    if(rows.isEmpty())
    {
      xml.writeStartElement(lit("tr"));

      for(int i = 0; i < cols.count(); i++)
      {
        xml.writeStartElement(lit("td"));
        xml.writeCharacters(lit("-"));
        xml.writeEndElement();
      }

      xml.writeEndElement();
    }
    else
    {
      for(const QVariantList &row : rows)
      {
        xml.writeStartElement(lit("tr"));

        for(const QVariant &el : row)
        {
          xml.writeStartElement(lit("td"));

          if(el.type() == QVariant::Bool)
            xml.writeCharacters(el.toBool() ? tr("True") : tr("False"));
          else
            xml.writeCharacters(el.toString());

          xml.writeEndElement();
        }

        xml.writeEndElement();
      }
    }

    xml.writeEndElement();
  }

  xml.writeEndElement();
}

void PipelineStateViewer::exportHTMLTable(QXmlStreamWriter &xml, const QStringList &cols,
                                          const QVariantList &row)
{
  exportHTMLTable(xml, cols, QList<QVariantList>({row}));
}

void PipelineStateViewer::endHTMLExport(QXmlStreamWriter *xml)
{
  xml->writeEndElement();    // </body>

  xml->writeEndElement();    // </html>

  xml->writeEndDocument();

  // delete the file the writer was writing to
  QFile *f = qobject_cast<QFile *>(xml->device());
  delete f;

  delete xml;
}

void PipelineStateViewer::setTopologyDiagram(QLabel *diagram, Topology topo)
{
  int idx = qMin((int)topo, (int)Topology::PatchList);

  if(m_TopoPixmaps[idx].isNull())
  {
    QSvgRenderer svg;
    switch(topo)
    {
      case Topology::PointList: svg.load(lit(":/topologies/topo_pointlist.svg")); break;
      case Topology::LineList: svg.load(lit(":/topologies/topo_linelist.svg")); break;
      case Topology::LineStrip: svg.load(lit(":/topologies/topo_linestrip.svg")); break;
      case Topology::TriangleList: svg.load(lit(":/topologies/topo_trilist.svg")); break;
      case Topology::TriangleStrip: svg.load(lit(":/topologies/topo_tristrip.svg")); break;
      case Topology::LineList_Adj: svg.load(lit(":/topologies/topo_linelist_adj.svg")); break;
      case Topology::LineStrip_Adj: svg.load(lit(":/topologies/topo_linestrip_adj.svg")); break;
      case Topology::TriangleList_Adj: svg.load(lit(":/topologies/topo_trilist_adj.svg")); break;
      case Topology::TriangleStrip_Adj: svg.load(lit(":/topologies/topo_tristrip_adj.svg")); break;
      default: svg.load(lit(":/topologies/topo_patch.svg")); break;
    }

    QRect rect = svg.viewBox();

    QImage im(rect.size() * diagram->devicePixelRatio(), QImage::Format_ARGB32);

    im.fill(QColor(0, 0, 0, 0));

    QPainter p(&im);

    svg.render(&p);

    // convert the colors - black maps to Text (foreground) and white maps to Base (background)
    QColor white = diagram->palette().color(QPalette::Active, QPalette::Base);
    QColor black = diagram->palette().color(QPalette::Active, QPalette::Text);

    const float br = black.redF();
    const float bg = black.greenF();
    const float bb = black.blueF();

    const float wr = white.redF();
    const float wg = white.greenF();
    const float wb = white.blueF();

    for(int y = 0; y < im.height(); y++)
    {
      QRgb *line = (QRgb *)im.scanLine(y);

      for(int x = 0; x < im.width(); x++)
      {
        // delta of 0 is black, delta of 255 is white
        const float delta = float(qRed(*line));
        const float bd = 255.0f - delta;
        const float wd = delta;

        const int r = int(br * bd + wr * wd);
        const int g = int(bg * bd + wg * wd);
        const int b = int(bb * bd + wb * wd);

        *line = qRgba(r, g, b, qAlpha(*line));

        line++;
      }
    }

    m_TopoPixmaps[idx] = QPixmap::fromImage(im);
    m_TopoPixmaps[idx].setDevicePixelRatio(diagram->devicePixelRatioF());
  }

  diagram->setPixmap(m_TopoPixmaps[idx]);
}

void PipelineStateViewer::setMeshViewPixmap(RDLabel *meshView)
{
  QImage meshIcon = Pixmaps::wireframe_mesh(meshView->devicePixelRatio()).toImage();
  QImage colSwapped(meshIcon.size(), QImage::Format_ARGB32);
  colSwapped.fill(meshView->palette().color(QPalette::WindowText));

  for(int y = 0; y < meshIcon.height(); y++)
  {
    const QRgb *in = (const QRgb *)meshIcon.constScanLine(y);
    QRgb *out = (QRgb *)colSwapped.scanLine(y);

    for(int x = 0; x < meshIcon.width(); x++)
    {
      *out = qRgba(qRed(*out), qGreen(*out), qBlue(*out), qAlpha(*in));

      in++;
      out++;
    }
  }

  QPixmap p = QPixmap::fromImage(colSwapped);
  p.setDevicePixelRatio(meshView->devicePixelRatioF());

  meshView->setPixmap(p);
  meshView->setPreserveAspectRatio(true);

  QPalette pal = meshView->palette();
  pal.setColor(QPalette::Shadow, pal.color(QPalette::Window).darker(120));
  meshView->setPalette(pal);
  meshView->setBackgroundRole(QPalette::Window);
  meshView->setMouseTracking(true);

  QObject::connect(meshView, &RDLabel::mouseMoved, [meshView](QMouseEvent *) {
    meshView->setBackgroundRole(QPalette::Shadow);
    meshView->setAutoFillBackground(true);
  });
  QObject::connect(meshView, &RDLabel::leave, [meshView]() {
    meshView->setBackgroundRole(QPalette::Window);
    meshView->setAutoFillBackground(false);
  });
}

void PipelineStateViewer::MakeShaderVariablesHLSL(bool cbufferContents,
                                                  const rdcarray<ShaderConstant> &vars,
                                                  QString &struct_contents, QString &struct_defs)
{
  for(const ShaderConstant &v : vars)
  {
    if(!v.type.members.isEmpty())
    {
      QString def = lit("struct %1 {\n").arg(v.type.descriptor.name);

      if(!struct_defs.contains(def))
      {
        QString contents;
        MakeShaderVariablesHLSL(false, v.type.members, contents, struct_defs);

        struct_defs += def + contents + lit("};\n\n");
      }
    }

    struct_contents += lit("\t%1 %2").arg(v.type.descriptor.name).arg(v.name);

    char comp = 'x';
    if(v.reg.comp == 1)
      comp = 'y';
    if(v.reg.comp == 2)
      comp = 'z';
    if(v.reg.comp == 3)
      comp = 'w';

    if(cbufferContents)
      struct_contents += lit(" : packoffset(c%1.%2);").arg(v.reg.vec).arg(QLatin1Char(comp));
    else
      struct_contents += lit(";");

    struct_contents += lit("\n");
  }
}

QString PipelineStateViewer::GenerateHLSLStub(const ShaderReflection *shaderDetails,
                                              const QString &entryFunc)
{
  QString hlsl = lit("// HLSL function stub generated\n\n");

  const QString textureDim[ENUM_ARRAY_SIZE(TextureType)] = {
      lit("Unknown"),          lit("Buffer"),      lit("Texture1D"),      lit("Texture1DArray"),
      lit("Texture2D"),        lit("TextureRect"), lit("Texture2DArray"), lit("Texture2DMS"),
      lit("Texture2DMSArray"), lit("Texture3D"),   lit("TextureCube"),    lit("TextureCubeArray"),
  };

  for(const ShaderSampler &samp : shaderDetails->samplers)
  {
    hlsl += lit("//SamplerComparisonState %1 : register(s%2); // can't disambiguate\n"
                "SamplerState %1 : register(s%2); // can't disambiguate\n")
                .arg(samp.name)
                .arg(samp.bindPoint);
  }

  for(int i = 0; i < 2; i++)
  {
    const rdcarray<ShaderResource> &resources =
        (i == 0 ? shaderDetails->readOnlyResources : shaderDetails->readWriteResources);
    for(const ShaderResource &res : resources)
    {
      char regChar = 't';

      if(i == 1)
      {
        hlsl += lit("RW");
        regChar = 'u';
      }

      if(res.isTexture)
      {
        hlsl += lit("%1<%2> %3 : register(%4%5);\n")
                    .arg(textureDim[(size_t)res.resType])
                    .arg(res.variableType.descriptor.name)
                    .arg(res.name)
                    .arg(QLatin1Char(regChar))
                    .arg(res.bindPoint);
      }
      else
      {
        if(res.variableType.descriptor.rows > 1)
          hlsl += lit("Structured");

        hlsl += lit("Buffer<%1> %2 : register(%3%4);\n")
                    .arg(res.variableType.descriptor.name)
                    .arg(res.name)
                    .arg(QLatin1Char(regChar))
                    .arg(res.bindPoint);
      }
    }
  }

  hlsl += lit("\n\n");

  QString cbuffers;

  int cbufIdx = 0;
  for(const ConstantBlock &cbuf : shaderDetails->constantBlocks)
  {
    if(!cbuf.name.isEmpty() && !cbuf.variables.isEmpty())
    {
      QString cbufName = cbuf.name;
      if(cbufName == lit("$Globals"))
        cbufName = lit("_Globals");
      cbuffers += lit("cbuffer %1 : register(b%2) {\n").arg(cbufName).arg(cbuf.bindPoint);
      MakeShaderVariablesHLSL(true, cbuf.variables, cbuffers, hlsl);
      cbuffers += lit("};\n\n");
    }
    cbufIdx++;
  }

  hlsl += cbuffers;

  hlsl += lit("\n\n");

  hlsl += lit("struct InputStruct {\n");
  for(const SigParameter &sig : shaderDetails->inputSignature)
    hlsl += lit("\t%1 %2 : %3;\n")
                .arg(TypeString(sig))
                .arg(!sig.varName.isEmpty() ? QString(sig.varName) : lit("param%1").arg(sig.regIndex))
                .arg(D3DSemanticString(sig));
  hlsl += lit("};\n\n");

  hlsl += lit("struct OutputStruct {\n");
  for(const SigParameter &sig : shaderDetails->outputSignature)
    hlsl += lit("\t%1 %2 : %3;\n")
                .arg(TypeString(sig))
                .arg(!sig.varName.isEmpty() ? QString(sig.varName) : lit("param%1").arg(sig.regIndex))
                .arg(D3DSemanticString(sig));
  hlsl += lit("};\n\n");

  hlsl += lit("OutputStruct %1(in InputStruct IN)\n"
              "{\n"
              "\tOutputStruct OUT = (OutputStruct)0;\n"
              "\n"
              "\t// ...\n"
              "\n"
              "\treturn OUT;\n"
              "}\n")
              .arg(entryFunc);

  return hlsl;
}

void PipelineStateViewer::shaderEdit_clicked()
{
  QToolButton *sender = qobject_cast<QToolButton *>(QObject::sender());
  if(!sender)
    return;

  // activate the first item in the menu, if there are any items, as the default action.
  QMenu *menu = sender->menu();
  if(menu && !menu->actions().isEmpty())
    menu->actions()[0]->trigger();
}

void PipelineStateViewer::EditShader(ResourceId id, ShaderStage shaderType, const rdcstr &entry,
                                     ShaderCompileFlags compileFlags, ShaderEncoding encoding,
                                     const rdcstrpairs &files)
{
  IShaderViewer *sv = m_Ctx.EditShader(
      false, shaderType, entry, files, encoding, compileFlags,
      // save callback
      [shaderType, id](ICaptureContext *ctx, IShaderViewer *viewer, ShaderEncoding shaderEncoding,
                       ShaderCompileFlags flags, rdcstr entryFunc, bytebuf shaderBytes) {

        if(shaderBytes.isEmpty())
          return;

        ANALYTIC_SET(UIFeatures.ShaderEditing, true);

        // invoke off to the ReplayController to replace the capture's shader
        // with our edited one
        ctx->Replay().AsyncInvoke([ctx, entryFunc, shaderBytes, shaderEncoding, flags, shaderType,
                                   id, viewer](IReplayController *r) {
          rdcstr errs;

          ResourceId from = id;
          ResourceId to;

          std::tie(to, errs) = r->BuildTargetShader(entryFunc.c_str(), shaderEncoding, shaderBytes,
                                                    flags, shaderType);

          GUIInvoke::call(viewer->Widget(), [viewer, errs]() { viewer->ShowErrors(errs); });
          if(to == ResourceId())
          {
            r->RemoveReplacement(from);
            GUIInvoke::call(viewer->Widget(), [ctx]() { ctx->RefreshStatus(); });
          }
          else
          {
            r->ReplaceResource(from, to);
            GUIInvoke::call(viewer->Widget(), [ctx]() { ctx->RefreshStatus(); });
          }
        });
      },

      // Close Callback
      [id](ICaptureContext *ctx) {
        // remove the replacement on close (we could make this more sophisticated if there
        // was a place to control replaced resources/shaders).
        ctx->Replay().AsyncInvoke([ctx, id](IReplayController *r) {
          r->RemoveReplacement(id);
          GUIInvoke::call(ctx->GetMainWindow()->Widget(), [ctx] { ctx->RefreshStatus(); });
        });
      });

  m_Ctx.AddDockWindow(sv->Widget(), DockReference::AddTo, this);
}

void PipelineStateViewer::EditOriginalShaderSource(ResourceId id,
                                                   const ShaderReflection *shaderDetails)
{
  QSet<uint> uniqueFiles;
  rdcstrpairs files;

  for(const ShaderSourceFile &s : shaderDetails->debugInfo.files)
  {
    QString filename = s.filename;

    uint filenameHash = qHash(filename.toLower());

    if(uniqueFiles.contains(filenameHash))
    {
      qWarning() << lit("Duplicate full filename") << filename;
      continue;
    }
    uniqueFiles.insert(filenameHash);

    files.push_back(make_rdcpair(s.filename, s.contents));
  }

  EditShader(id, shaderDetails->stage, shaderDetails->entryPoint,
             shaderDetails->debugInfo.compileFlags, shaderDetails->debugInfo.encoding, files);
}

void PipelineStateViewer::EditDecompiledSource(const ShaderProcessingTool &tool, ResourceId id,
                                               const ShaderReflection *shaderDetails)
{
  QString source = tool.DisassembleShader(this, shaderDetails, "");

  if(source.isEmpty())
    return;

  rdcstrpairs files;
  files.push_back(make_rdcpair<rdcstr, rdcstr>("decompiled", source));

  EditShader(id, shaderDetails->stage, shaderDetails->entryPoint,
             shaderDetails->debugInfo.compileFlags, tool.output, files);
}

void PipelineStateViewer::SetupShaderEditButton(QToolButton *button, ResourceId pipelineId,
                                                ResourceId shaderId,
                                                const ShaderReflection *shaderDetails)
{
  if(!shaderDetails || !button->isEnabled() || button->popupMode() != QToolButton::MenuButtonPopup)
    return;

  QMenu *menu = editMenus[(int)shaderDetails->stage];

  menu->clear();

  rdcarray<ShaderEncoding> accepted = m_Ctx.TargetShaderEncodings();

  // if we have original source and it's in a known format, display it as the first most preferred
  // option
  if(!shaderDetails->debugInfo.files.empty() &&
     shaderDetails->debugInfo.encoding != ShaderEncoding::Unknown)
  {
    QAction *action =
        new QAction(tr("Edit Source - %1").arg(shaderDetails->debugInfo.files[0].filename), menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, shaderId, shaderDetails]() {
      EditOriginalShaderSource(shaderId, shaderDetails);
    });

    menu->addAction(action);
  }

  // next up, try the shader processing tools in order - all the ones that will decompile from our
  // native representation. We don't check here yet if we have a valid compiler to compile from the
  // output to what we want.
  for(const ShaderProcessingTool &tool : m_Ctx.Config().ShaderProcessors)
  {
    // skip tools that can't decode our shader, or doesn't produce a textual output
    if(tool.input != shaderDetails->encoding || !IsTextRepresentation(tool.output))
      continue;

    QAction *action = new QAction(tr("Decompile with %1").arg(tool.name), menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, tool, shaderId, shaderDetails]() {
      EditDecompiledSource(tool, shaderId, shaderDetails);
    });

    menu->addAction(action);
  }

  // if all else fails we can generate a stub for editing. Skip this for GLSL as it always has
  // source above which is preferred.
  if(shaderDetails->encoding != ShaderEncoding::GLSL)
  {
    QString label = tr("Edit Generated Stub");

    if(shaderDetails->encoding == ShaderEncoding::SPIRV)
      label = tr("Edit Pseudocode");

    QAction *action = new QAction(label, menu);
    action->setIcon(Icons::page_white_edit());

    QObject::connect(action, &QAction::triggered, [this, pipelineId, shaderId, shaderDetails]() {
      QString entry;
      QString src;

      if(shaderDetails->encoding == ShaderEncoding::SPIRV)
      {
        m_Ctx.Replay().AsyncInvoke([this, pipelineId, shaderId, shaderDetails](IReplayController *r) {
          rdcstr disasm = r->DisassembleShader(pipelineId, shaderDetails, "");

          QString editeddisasm =
              tr("####          PSEUDOCODE SPIR-V DISASSEMBLY            ###\n") +
              tr("#### Use a SPIR-V decompiler to get compileable source ###\n\n");

          editeddisasm += disasm;

          GUIInvoke::call(this, [this, shaderId, shaderDetails, editeddisasm]() {
            rdcstrpairs files;
            files.push_back(make_rdcpair<rdcstr, rdcstr>("pseudocode", editeddisasm));

            EditShader(shaderId, shaderDetails->stage, shaderDetails->entryPoint,
                       ShaderCompileFlags(), ShaderEncoding::Unknown, files);
          });
        });
      }
      else if(shaderDetails->encoding == ShaderEncoding::DXBC)
      {
        entry = lit("EditedShader%1S").arg(ToQStr(shaderDetails->stage, GraphicsAPI::D3D11)[0]);

        rdcstrpairs files;
        files.push_back(make_rdcpair<rdcstr, rdcstr>("decompiled_stub.hlsl",
                                                     GenerateHLSLStub(shaderDetails, entry)));

        EditShader(shaderId, shaderDetails->stage, entry, ShaderCompileFlags(),
                   ShaderEncoding::HLSL, files);
      }

    });

    menu->addAction(action);
  }

  button->setMenu(menu);
}

bool PipelineStateViewer::SaveShaderFile(const ShaderReflection *shader)
{
  if(!shader)
    return false;

  QString filter;

  if(m_Ctx.CurPipelineState().IsCaptureD3D11() || m_Ctx.CurPipelineState().IsCaptureD3D12())
  {
    filter = tr("DXBC Shader files (*.dxbc)");
  }
  else if(m_Ctx.CurPipelineState().IsCaptureGL())
  {
    filter = tr("GLSL files (*.glsl)");
  }
  else if(m_Ctx.CurPipelineState().IsCaptureVK())
  {
    filter = tr("SPIR-V files (*.spv)");
  }

  QString filename = RDDialog::getSaveFileName(this, tr("Save Shader As"), QString(), filter);

  if(!filename.isEmpty())
  {
    ANALYTIC_SET(Export.Shader, true);

    QDir dirinfo = QFileInfo(filename).dir();
    if(dirinfo.exists())
    {
      QFile f(filename);
      if(f.open(QIODevice::WriteOnly | QIODevice::Truncate))
      {
        f.write((const char *)shader->rawBytes.data(), (qint64)shader->rawBytes.size());
      }
      else
      {
        RDDialog::critical(
            this, tr("Error saving shader"),
            tr("Couldn't open path %1 for write.\n%2").arg(filename).arg(f.errorString()));
        return false;
      }
    }
    else
    {
      RDDialog::critical(this, tr("Invalid directory"),
                         tr("Cannot find target directory to save to"));
      return false;
    }
  }

  return true;
}
