plot.roc = function(p, p0, fpr = seq(0,1,by=0.01), col="black", xlim=c(0,1), ylim=c(0,1), main="",lty=1, lwd=2) { ## helper function to plot ROC

  plot(fpr,ecdf(p)(quantile(p0,fpr)), type="l", lty=lty, xlab="False rejection rate", ylab="True rejection rate", col=col, xlim=xlim, ylim=ylim, main=main,lwd=lwd)
}

library(grid)
plot.part=function(xy.part,border=FALSE,xlim=c(0,1), ylim=c(0,1), zlim=extendrange(xy.part[,"den"]), main="" ,nlevels=100,color.fun=terrain.colors,plot.scale=TRUE,color=NA) {
  if (zlim[1] < 0) zlim[1]=0
  
  colors = rev(color.fun(nlevels))
  fill.colors = colors[round((zlim[2]-xy.part[,"den"])/(zlim[2]-zlim[1])*nlevels)]
  if (border) border.col = "black" else border.col=fill.colors
  graphic.params=gpar(col=border.col,xlim=xlim,ylim=ylim,fill=fill.colors)

  xy.part[,"xmin"]=pmax(xy.part[,"xmin"],xlim[1]);xy.part[,"xmax"]=pmax(xy.part[,"xmax"],xlim[1])
  xy.part[,"xmin"]=pmin(xy.part[,"xmin"],xlim[2]);xy.part[,"xmax"]=pmin(xy.part[,"xmax"],xlim[2])
  xy.part[,"ymin"]=pmax(xy.part[,"ymin"],ylim[1]);xy.part[,"ymax"]=pmax(xy.part[,"ymax"],ylim[1])
  xy.part[,"ymin"]=pmin(xy.part[,"ymin"],ylim[2]);xy.part[,"ymax"]=pmin(xy.part[,"ymax"],ylim[2]) 
    
  
  
  if (plot.scale) {
    vpPlot = viewport(x=unit(4,"lines"), y=unit(0.2,"npc")+unit(2,"lines"), just=c("left","bottom"),
      width = unit(1,"npc")-unit(6,"lines"), height=unit(0.70,"npc"), xscale=xlim,yscale=ylim,name="plotRegion")
    vpScale = viewport(x=unit(4,"lines"), y = unit(4,"lines"),just=c("left","bottom"),width= unit(1,"npc")-unit(6,"lines"),
      height=unit(0.2,"npc"),name="scaleRegion")
    
    pushViewport(vpPlot)
    
    
    grid.rect(unit((xy.part[,"xmin"]+xy.part[,"xmax"])/2,"native"),unit((xy.part[,"ymin"]+xy.part[,"ymax"])/2,"native"),
              width=unit(xy.part[,"xmax"]-xy.part[,"xmin"],"native"),height=unit(xy.part[,"ymax"]-xy.part[,"ymin"],"native"),
              draw=TRUE,gp=graphic.params)
    
    
    ## grid.xaxis()
    ## grid.yaxis()
    popViewport()
    
                                        

    pushViewport(vpScale);
  
    height=1/(nlevels-1);
  
    grid.rect(y=unit(0,"npc"),x=unit((0:(nlevels-2))*height,"npc"),height=unit(0.2,"npc"),width=unit(height,"npc"),
              just=c("left","bottom"),gp=gpar(col=0,fill=colors),draw=TRUE)
    grid.xaxis(at=c(0,0.5,1),label=round(c(zlim[1],mean(zlim),zlim[2]),2))
    grid.text("Density scale",y=unit(0.1,"npc"))
    popViewport()
  }

  else {
    pushViewport(plotViewport(c(5,4,2,2)))
    pushViewport(viewport(xscale=xlim,yscale=ylim,name="plotRegion"))
    
    grid.rect(unit((xy.part[,"xmin"]+xy.part[,"xmax"])/2,"native"),unit((xy.part[,"ymin"]+xy.part[,"ymax"])/2,"native"),
              width=unit(xy.part[,"xmax"]-xy.part[,"xmin"],"native"),height=unit(xy.part[,"ymax"]-xy.part[,"ymin"],"native"),
              draw=TRUE,gp=graphic.params)

    
    ## grid.xaxis()
    ## grid.yaxis()
    
    popViewport(2)
  }

  grid.text(main,y=unit(1,"npc")-unit(1,"lines"))
               
}
  

