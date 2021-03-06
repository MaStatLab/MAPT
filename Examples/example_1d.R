x=seq(from=0.0001,to=0.9999,by=0.002)

nobs = 500 
max.dim=11
size.max = nobs

p1=0.1;p2=0.3;p3=0.4;p4=0.2
sample.ind = runif(size.max)
  
obs.mat = as.matrix((sample.ind<=p1)*runif(size.max)+(p1<sample.ind & sample.ind<=p1+p2)*(0.25+rbeta(size.max,1,1)*0.25)+(p1+p2<sample.ind & sample.ind<=p1+p2+p3)*(0.25+rbeta(size.max,2,2)*0.25) +(sample.ind>p1+p2+p3)*rbeta(size.max,5000,2000))
  
true.den = p1*dunif(x) + p2*dunif(x,0.25,0.5) + p3 *4*dbeta((x-0.25)*4,2,2) + p4*dbeta(x,5000,2000)  


pred.den = markov.apt.density.kD(obs.mat,max.dim=max.dim,rho0=0,rho0.mode=0,n.s=3,tran.mode=2,beta=0,lognu.lb=-1,lognu.ub=4,n.grid=5,x.mat.pred=x)$ppd

plot(x,pred.den,type='l')
par(new=TRUE)
plot(x,true.den,type='l',lty=2,col='red')






